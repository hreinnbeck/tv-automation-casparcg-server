/*
* Copyright (c) 2011 Sveriges Television AB <info@casparcg.com>
*
* This file is part of CasparCG (www.casparcg.com).
*
* CasparCG is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* CasparCG is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with CasparCG. If not, see <http://www.gnu.org/licenses/>.
*
* Author: Robert Nagy, ronag89@gmail.com
*/

// TODO: Smart GC

#include "../../StdAfx.h"

#include "device.h"

#include "buffer.h"
#include "texture.h"
#include "shader.h"

#include <common/assert.h>
#include <common/except.h>
#include <common/future.h>
#include <common/array.h>
#include <common/memory.h>
#include <common/gl/gl_check.h>
#include <common/timer.h>

#include <GL/glew.h>

#include <SFML/Window/Context.hpp>

#ifdef WIN32
#include "../../d3d/d3d_device.h"
#include <GL/wglew.h>
#endif

#include <tbb/concurrent_unordered_map.h>
#include <tbb/concurrent_hash_map.h>
#include <tbb/concurrent_queue.h>

#include <boost/utility/declval.hpp>
#include <boost/property_tree/ptree.hpp>

#include <array>
#include <unordered_map>

#include <tbb/parallel_for.h>

namespace caspar { namespace accelerator { namespace ogl {
		
struct device::impl : public std::enable_shared_from_this<impl>
{	
	static_assert(std::is_same<decltype(boost::declval<device>().impl_), spl::shared_ptr<impl>>::value, "impl_ must be shared_ptr");

	tbb::concurrent_hash_map<buffer*, std::shared_ptr<texture>> texture_cache_;

	std::unique_ptr<sf::Context> device_;
	
#ifdef WIN32
    std::shared_ptr<d3d::d3d_device> d3d_device_;
    std::shared_ptr<void>            interop_handle_;
#endif
	
	std::array<tbb::concurrent_unordered_map<std::size_t, tbb::concurrent_bounded_queue<std::shared_ptr<texture>>>, 8>	device_pools_;
	std::array<tbb::concurrent_unordered_map<std::size_t, tbb::concurrent_bounded_queue<std::shared_ptr<buffer>>>, 2>	host_pools_;
	
	GLuint fbo_;

	executor& executor_;
				
	impl(executor& executor) 
		: executor_(executor)
	{
		executor_.set_capacity(256);

		CASPAR_LOG(info) << L"Initializing OpenGL Device.";
		
		executor_.invoke([=]
		{
			device_.reset(new sf::Context());
			device_->setActive(true);		
						
			if (glewInit() != GLEW_OK)
				CASPAR_THROW_EXCEPTION(gl::ogl_exception() << msg_info("Failed to initialize GLEW."));
#ifdef WIN32
			if (wglewInit() != GLEW_OK)
				CASPAR_THROW_EXCEPTION(gl::ogl_exception() << msg_info("Failed to initialize WGLEW."));
#endif

			if(!GLEW_VERSION_4_3)
				CASPAR_THROW_EXCEPTION(not_supported() << msg_info("Your graphics card does not meet the minimum hardware requirements since it does not support OpenGL 3.0 or higher."));
	
			glGenFramebuffers(1, &fbo_);				
			glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
			
#ifdef WIN32
		d3d_device_ = d3d::d3d_device::get_device();
        if (d3d_device_) {
            interop_handle_ = std::shared_ptr<void>(wglDXOpenDeviceNV(d3d_device_->device()), [](void* p) {
 				if (p)
                    wglDXCloseDeviceNV(p);
            });

            if (!interop_handle_)
                CASPAR_THROW_EXCEPTION(gl::ogl_exception() << msg_info("Failed to initialize d3d interop."));
        }
#endif
		});
				
		CASPAR_LOG(info) << L"Successfully initialized OpenGL " << version();
	}

	~impl()
	{
		auto context = executor_.is_current() ? std::string() : get_context();

		executor_.invoke([=]
		{
			CASPAR_SCOPED_CONTEXT_MSG(context);
			texture_cache_.clear();

			for (auto& pool : host_pools_)
				pool.clear();

			for (auto& pool : device_pools_)
				pool.clear();

			glDeleteFramebuffers(1, &fbo_);

			device_.reset();
		});
	}

	boost::property_tree::wptree info() const
	{
		boost::property_tree::wptree info;

		boost::property_tree::wptree pooled_device_buffers;
		size_t total_pooled_device_buffer_size	= 0;
		size_t total_pooled_device_buffer_count	= 0;

		for (size_t i = 0; i < device_pools_.size(); ++i)
		{
			auto& pools		= device_pools_.at(i);
			bool mipmapping	= i > 3;
			auto stride		= mipmapping ? i - 3 : i + 1;

			for (auto& pool : pools)
			{
				auto width	= pool.first >> 16;
				auto height	= pool.first & 0x0000FFFF;
				auto size	= width * height * stride;
				auto count	= pool.second.size();

				if (count == 0)
					continue;

				boost::property_tree::wptree pool_info;

				pool_info.add(L"stride",		stride);
				pool_info.add(L"mipmapping",	mipmapping);
				pool_info.add(L"width",			width);
				pool_info.add(L"height",		height);
				pool_info.add(L"size",			size);
				pool_info.add(L"count",			count);

				total_pooled_device_buffer_size		+= size * count;
				total_pooled_device_buffer_count	+= count;

				pooled_device_buffers.add_child(L"device_buffer_pool", pool_info);
			}
		}

		info.add_child(L"gl.details.pooled_device_buffers", pooled_device_buffers);

		boost::property_tree::wptree pooled_host_buffers;
		size_t total_read_size		= 0;
		size_t total_write_size		= 0;
		size_t total_read_count		= 0;
		size_t total_write_count	= 0;

		for (size_t i = 0; i < host_pools_.size(); ++i)
		{
			auto& pools	= host_pools_.at(i);
			auto usage	= static_cast<buffer::usage>(i);

			for (auto& pool : pools)
			{
				auto size	= pool.first;
				auto count	= pool.second.size();

				if (count == 0)
					continue;

				boost::property_tree::wptree pool_info;

				pool_info.add(L"usage",	usage == buffer::usage::read_only ? L"read_only" : L"write_only");
				pool_info.add(L"size",	size);
				pool_info.add(L"count",	count);

				pooled_host_buffers.add_child(L"host_buffer_pool", pool_info);

				(usage == buffer::usage::read_only ? total_read_count : total_write_count) += count;
				(usage == buffer::usage::read_only ? total_read_size : total_write_size) += size * count;
			}
		}

		info.add_child(L"gl.details.pooled_host_buffers",				pooled_host_buffers);
		info.add(L"gl.summary.pooled_device_buffers.total_count",		total_pooled_device_buffer_count);
		info.add(L"gl.summary.pooled_device_buffers.total_size",		total_pooled_device_buffer_size);
		info.add_child(L"gl.summary.all_device_buffers",				texture::info());
		info.add(L"gl.summary.pooled_host_buffers.total_read_count",	total_read_count);
		info.add(L"gl.summary.pooled_host_buffers.total_write_count",	total_write_count);
		info.add(L"gl.summary.pooled_host_buffers.total_read_size",		total_read_size);
		info.add(L"gl.summary.pooled_host_buffers.total_write_size",	total_write_size);
		info.add_child(L"gl.summary.all_host_buffers",					buffer::info());

		return info;
	}
		
	std::wstring version()
	{	
		try
		{
			return executor_.invoke([]
			{
				return u16(reinterpret_cast<const char*>(GL2(glGetString(GL_VERSION)))) + L" " + u16(reinterpret_cast<const char*>(GL2(glGetString(GL_VENDOR))));
			});	
		}
		catch(...)
		{
			return L"Not found";;
		}
	}

	tbb::concurrent_bounded_queue<std::shared_ptr<texture>>* get_texture_pool(int width, int height, int stride, bool mipmapped) {
		return &device_pools_[stride - 1 + (mipmapped ? 4 : 0)][((width << 16) & 0xFFFF0000) | (height & 0x0000FFFF)];
	}
							
	spl::shared_ptr<texture> create_texture(int width, int height, int stride, bool mipmapped, bool clear)
	{
		CASPAR_VERIFY(stride > 0 && stride < 5);
		CASPAR_VERIFY(width > 0 && height > 0);

		if(!executor_.is_current())
			CASPAR_THROW_EXCEPTION(invalid_operation() << msg_info("Operation only valid in an OpenGL Context."));
					
		auto pool = get_texture_pool(width, height, stride, mipmapped);
		
		std::shared_ptr<texture> tex;
		if (!pool->try_pop(tex)) {
			tex = spl::make_shared<texture>(width, height, stride, mipmapped);
			CASPAR_LOG(debug) << L"[texture] Texture allocation: " << width << L"x" << height << L"x" << stride;
		}
	
		if(clear)
			tex->clear();

		return spl::shared_ptr<texture>(tex.get(), [tex, pool](texture*) mutable
		{		
			pool->push(tex);	
		});
	}
		
	spl::shared_ptr<buffer> create_buffer(std::size_t size, buffer::usage usage)
	{
		CASPAR_VERIFY(size > 0);
		
		auto pool = &host_pools_[static_cast<int>(usage)][size];
		
		std::shared_ptr<buffer> buf;
		if(!pool->try_pop(buf))	
		{
			caspar::timer timer;

			auto context = executor_.is_current() ? std::string() : get_context();

			// Prioritise the mix target, so that mixing can flow smoothly
			auto priority = usage == buffer::usage::read_only ? task_priority::high_priority : task_priority::normal_priority;

			buf = executor_.invoke([&]
			{
				CASPAR_SCOPED_CONTEXT_MSG(context);
				caspar::timer timer;

				auto buf = std::make_shared<buffer>(size, usage);

				if (timer.elapsed() > 0.02)
					CASPAR_LOG(warning) << L"[buffer] Performance warning. Buffer allocation (" << size << L"b) blocked: " << timer.elapsed();
				else
					CASPAR_LOG(debug) << L"[buffer] Buffer allocation (" << size << L"b) took: " << timer.elapsed();

				return buf;
			}, priority);
			
			if(timer.elapsed() > 0.02)
				CASPAR_LOG(warning) << L"[ogl-device] Performance warning. Buffer allocation blocked: " << timer.elapsed();
		}
		
		std::weak_ptr<impl> self = shared_from_this(); // buffers can leave the device context, take a hold on life-time.
		return spl::shared_ptr<buffer>(buf.get(), [=](buffer*) mutable
		{
			auto strong = self.lock();

			if (strong)
			{
				auto context = executor_.is_current() ? std::string() : get_context();

				strong->executor_.invoke([&]
				{
					CASPAR_SCOPED_CONTEXT_MSG(context);
					strong->texture_cache_.erase(buf.get());
					// Clear any previous data
					// TODO - does this want to clear the buffer?
				}, task_priority::high_priority);
				
				pool->push(buf);
			}
			else
			{
				CASPAR_LOG(info) << L"Buffer outlived ogl device";
			}
		});
	}

	array<std::uint8_t> create_array(std::size_t size)
	{		
		auto buf = create_buffer(size, buffer::usage::write_only);
		return array<std::uint8_t>(buf->data(), buf->size(), false, buf);
	}

	template<typename T>
	std::shared_ptr<buffer> copy_to_buf(const T& source)
	{
		std::shared_ptr<buffer> buf;

		auto tmp = source.template storage<spl::shared_ptr<buffer>>();
		if(tmp)
			buf = *tmp;
		else
		{			
			buf = create_buffer(source.size(), buffer::usage::write_only);
			tbb::parallel_for(tbb::blocked_range<std::size_t>(0, source.size()), [&](const tbb::blocked_range<std::size_t>& r)
			{
				std::memcpy(buf->data() + r.begin(), source.data() + r.begin(), r.size());
			});
		}

		return buf;
	}

	// TODO: Since the returned texture is cached it SHOULD NOT be modified.
	std::future<std::shared_ptr<texture>> copy_async(const array<const std::uint8_t>& source, int width, int height, int stride, bool mipmapped)
	{
		std::shared_ptr<buffer> buf = copy_to_buf(source);
		auto context = executor_.is_current() ? std::string() : get_context();

		return executor_.begin_invoke([=]() -> std::shared_ptr<texture>
		{
			CASPAR_SCOPED_CONTEXT_MSG(context);
			tbb::concurrent_hash_map<buffer*, std::shared_ptr<texture>>::const_accessor a;
			if(texture_cache_.find(a, buf.get()))
				return spl::make_shared_ptr(a->second);

			auto texture = create_texture(width, height, stride, mipmapped, false);
			texture->copy_from(*buf);

			texture_cache_.insert(std::make_pair(buf.get(), texture));
			
			return texture;
		}); // , task_priority::high_priority);
	}
	
	//std::future<std::shared_ptr<texture>> copy_async(const array<std::uint8_t>& source, int width, int height, int stride, bool mipmapped)
	//{
	//	std::shared_ptr<buffer> buf = copy_to_buf(source);
	//	auto context = executor_.is_current() ? std::string() : get_context();

	//	return executor_.begin_invoke([=]() -> std::shared_ptr<texture>
	//	{
	//		CASPAR_SCOPED_CONTEXT_MSG(context);
	//		auto texture = create_texture(width, height, stride, mipmapped, false);
	//		texture->copy_from(*buf);	
	//		
	//		return texture;
	//	}); //, task_priority::high_priority);
	//}

	std::future<array<const std::uint8_t>> copy_async(const spl::shared_ptr<texture>& source)
	{
		if(!executor_.is_current())
			CASPAR_THROW_EXCEPTION(invalid_operation() << msg_info("Operation only valid in an OpenGL Context."));

		auto buffer = create_buffer(source->size(), buffer::usage::read_only); 
		source->copy_to(*buffer);	

		GLsync fence = GL2(glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0));
		GL(glFlush());

		auto self = shared_from_this();
		auto context = get_context();
		auto cmd = [self, buffer, context, fence]() mutable -> array<const std::uint8_t>
		{
			self->executor_.invoke([&buffer, &context, &fence] // Defer blocking call until data is needed.
			{
				caspar::timer timer;

				// TODO - smarter timeout?
				if (GL2(glClientWaitSync(fence, 0, 1000000000)) == GL_TIMEOUT_EXPIRED) {
					CASPAR_LOG(warning) << L"[copy_async] Fence wait timed out";
				}

				GL(glDeleteSync(fence));

				if (timer.elapsed() > 0.02)
					CASPAR_LOG(warning) << L"[buffer] Performance warning. Buffer mapping blocked: " << timer.elapsed();
			});
			return array<const std::uint8_t>(buffer->data(), buffer->size(), true, buffer);
		};
		return std::async(std::launch::deferred, std::move(cmd));
	}
	
#ifdef WIN32
    std::future<std::shared_ptr<texture>> copy_async(uint32_t source, int width, int height, int stride)
    {
		if (!executor_.is_current())
			CASPAR_THROW_EXCEPTION(invalid_operation() << msg_info("Operation only valid in an OpenGL Context."));

        auto tex = create_texture(width, height, stride, false, false); // TODO - mipmap?

        tex->copy_from(source);

        auto fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

        GL(glFlush());

		auto self = shared_from_this();
		auto context = get_context();
		auto cmd = [self, tex, context, fence]() mutable->std::shared_ptr<texture>
		{
			self->executor_.invoke([&tex, &context, &fence] // Defer blocking call until data is needed.
				{
					caspar::timer timer;

					// TODO - smarter timeout?
					if (GL2(glClientWaitSync(fence, 0, 1000000000)) == GL_TIMEOUT_EXPIRED) {
						CASPAR_LOG(warning) << L"[copy_async] Fence wait timed out";
					}

					GL(glDeleteSync(fence));

					if (timer.elapsed() > 0.02)
						CASPAR_LOG(warning) << L"[buffer] Performance warning. Texture copy blocked: " << timer.elapsed();
				});
			return tex;
		};
		return std::async(std::launch::deferred, std::move(cmd));
    }
#endif

	std::future<void> gc()
	{
		return executor_.begin_invoke([=]
		{
			CASPAR_LOG(info) << " ogl: Running GC.";

			try
			{
				for (auto& pools : device_pools_)
				{
					for (auto& pool : pools)
						pool.second.clear();
				}
				for (auto& pools : host_pools_)
				{
					for (auto& pool : pools)
						pool.second.clear();
				}
			}
			catch (...)
			{
				CASPAR_LOG_CURRENT_EXCEPTION();
			}
		}, task_priority::high_priority);
	}

	void allocate_buffers(int count, int width, int height, int depth, bool mipmapped, bool for_channel) {
		buffer::usage usage = for_channel ? buffer::usage::read_only : buffer::usage::write_only;
		executor_.invoke([&]() {
			int size = width * height * depth;
			auto buffer_pool = &host_pools_[static_cast<int>(usage)][size];
			auto texture_pool = get_texture_pool(width, height, depth, mipmapped);

			for (int i = 0; i < count; i++) {
				buffer_pool->push(std::make_shared<buffer>(size, usage));
				texture_pool->push(std::make_shared<texture>(width, height, depth, mipmapped));
			}
		});
	}
};

device::device() 
	: executor_(L"OpenGL Rendering Context")
	, impl_(new impl(executor_)){
}
device::~device(){}
spl::shared_ptr<texture>					device::create_texture(int width, int height, int stride, bool mipmapped){ return impl_->create_texture(width, height, stride, mipmapped, true); }
array<std::uint8_t>							device::create_array(int size){return impl_->create_array(size);}
void										device::allocate_buffers(int count, int width, int height, int depth, bool mipmapped, bool for_channel) { impl_->allocate_buffers(count, width, height, depth, mipmapped, for_channel); }
std::future<std::shared_ptr<texture>>		device::copy_async(const array<const std::uint8_t>& source, int width, int height, int stride, bool mipmapped){return impl_->copy_async(source, width, height, stride, mipmapped);}
//std::future<std::shared_ptr<texture>>		device::copy_async(const array<std::uint8_t>& source, int width, int height, int stride, bool mipmapped){ return impl_->copy_async(source, width, height, stride, mipmapped); }
std::future<array<const std::uint8_t>>		device::copy_async(const spl::shared_ptr<texture>& source){return impl_->copy_async(source);}
std::future<void>							device::gc() { return impl_->gc(); }
boost::property_tree::wptree				device::info() const { return impl_->info(); }
std::wstring								device::version() const{return impl_->version();}

#ifdef WIN32
std::shared_ptr<void>                 device::d3d_interop() const { return impl_->interop_handle_; }
std::future<std::shared_ptr<texture>> device::copy_async(uint32_t source, int width, int height, int stride)
{
    return impl_->copy_async(source, width, height, stride);
}
#endif

}}}
