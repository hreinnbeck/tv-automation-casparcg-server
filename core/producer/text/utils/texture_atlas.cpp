/* ============================================================================
 * Freetype GL - A C OpenGL Freetype engine
 * Platform:    Any
 * WWW:         http://code.google.com/p/freetype-gl/
 * ----------------------------------------------------------------------------
 * Copyright 2011,2012 Nicolas P. Rougier. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NICOLAS P. ROUGIER ''AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL NICOLAS P. ROUGIER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of Nicolas P. Rougier.
 * ============================================================================
 */

#include <list>
#include <vector>
#include "texture_atlas.h"

namespace caspar { namespace core { namespace text {

class texture_atlas
{
private:
	struct node
	{
		int x;
		int y;
		int width;
	};

	typedef std::list<node>::iterator node_iterator;

	std::list<node> nodes_;

	size_t width_;
	size_t height_;
	size_t depth_;
	std::vector<uint8_t> data_;

public:
	texture_atlas(const size_t width, const size_t height, const size_t depth) : width_(width), height_(height), depth_(depth), data_(width*height*depth, 0)
	{
		// We want a one pixel border around the whole atlas to avoid any artefact when sampling texture
		const node n = {5, 5, static_cast<int>(width_) - 2};
		nodes_.push_back(n);
	}

	atlas_rect get_region(const int width, const int height)
	{
		atlas_rect region = { -1, 0, 0, width, height };

		int best_height = INT_MAX;
		int best_width = INT_MAX;
		node_iterator best_it = nodes_.end();

		for(auto it = nodes_.begin(); it != nodes_.end(); ++it)
		{
			const int y = fit(it, width, height);
			if( y >= 0 )
			{
				if( y + height < best_height ||
					( y + height == best_height && it->width < best_width) )
				{
					best_height = y + height;
					best_it = it;
					best_width = it->width;
					region.x = it->x;
					region.y = y;
				}
			}
		}

		if(best_it == nodes_.end())
		{
			region.x = -1;
			region.y = -1;
			region.width = 0;
			region.height = 0;
			return region;
		}

		node new_node;
		new_node.x = region.x;
		new_node.y = region.y + height;
		new_node.width = width;

		best_it = nodes_.insert(best_it, new_node);

		for(auto it = ++best_it; it != nodes_.end(); ++it)
		{
			auto prev = it; --prev;

			if (it->x < prev->x + prev->width )
			{
				const int shrink = prev->x + prev->width - it->x;
				it->x += shrink;
				it->width -= shrink;
				if (it->width <= 0)
				{
					nodes_.erase(it);
					it = prev;
				}
				else
					break;
			}
			else
				break;
		}

		merge();
		return region;
	}

	//the data parameter points to bitmap-data that is 8-bit grayscale.
	void set_region(size_t x, size_t y, const size_t width, const size_t height, const unsigned char *src, const size_t stride, const color<double>& col)
	{
		// Pad to ensure no edges get cut harshly, and no bleed
		x += CHAR_PADDING;
		y += CHAR_PADDING;

		//this assumes depth_ is set to 4
		for(size_t i=0; i<height; ++i)
		{
			for(size_t j=0; j<width; ++j)
			{
				unsigned char* pixel = &data_[((y+i)*width_ + x + j)*depth_];
				unsigned char value = src[i*stride + j];
				pixel[0] = (unsigned char)(value*col.b);
				pixel[1] = (unsigned char)(value*col.g);
				pixel[2] = (unsigned char)(value*col.r);
				pixel[3] = (unsigned char)(value*col.a);
			}
		}
	}

	size_t depth() const { return depth_; }
	size_t width() const { return width_; }
	size_t height() const { return height_; }
	const uint8_t* data() const { return data_.data(); }

private:
	int fit(node_iterator it, const size_t width, const size_t height)
	{
		int x = it->x;
		int y = it->y;
		int width_left = static_cast<int>(width);

		if ((x + width) > (width_ - 1))
			return -1;

		while( width_left > 0 && it != nodes_.end())
		{
			if(it->y > y)
				y = it->y;
			if(y + height > (height_ - 1))
				return -1;

			width_left -= it->width;
			++it;
		}

		return y;
	}

	void merge()
	{
		auto it = nodes_.begin();
		while(true)
		{
			auto next = it; ++next;
			if(next == nodes_.end())
				break;

			if(it->y == (*next).y)
			{
				it->width += (*next).width;
				nodes_.erase(next);
			}
			else
				++it;
		}
	}
};

struct texture_atlas_set::impl
{
private:
	size_t width_;
	size_t height_;
	size_t depth_;
	std::vector<std::shared_ptr<texture_atlas>> data_;

public:
	impl(const size_t width, const size_t height, const size_t depth) : width_(width), height_(height), depth_(depth), data_()
	{
		data_.push_back(std::make_shared<texture_atlas>(width, height, depth));
	}

	atlas_rect get_region(const int width, const int height)
	{
		if (width >= width_ || height >= height_)
			return atlas_rect { -1, -1, -1, width, height };

		for (auto it = data_.begin(); it != data_.end(); ++it)
		{
			atlas_rect res = it->get()->get_region(width, height);
			if (res.x != -1 && res.y != -1) {
				res.index = static_cast<int>(it - data_.begin());
				return res;
			}
		}

		auto new_page = std::make_shared<texture_atlas>(width_, height_, depth_);
		auto res = new_page->get_region(width, height);
		data_.push_back(std::move(new_page));
		res.index = static_cast<int>(data_.size() - 1);
		return res;
	}

	//the data parameter points to bitmap-data that is 8-bit grayscale.
	void set_region(const atlas_rect rect, const size_t width, const size_t height, const unsigned char *src, const size_t stride, const color<double>& col)
	{
		if (rect.index > data_.size())
			return; // TODO - throw error?

		data_.at(rect.index)->set_region(rect.x, rect.y, width, height, src, stride, col);
	}

	size_t depth() const { return depth_; }
	size_t width() const { return width_; }
	size_t height() const { return height_; }

	size_t size() const { return data_.size(); }
	const uint8_t* data(const int index) const { return data_.at(index)->data(); }

};


texture_atlas_set::texture_atlas_set(const size_t w, const size_t h, const size_t d) : impl_(new impl(w, h, d)) {}
atlas_rect texture_atlas_set::get_region(const int width, const int height) const { return impl_->get_region(width, height); }
void texture_atlas_set::set_region(const atlas_rect rect, const size_t width, const size_t height, const unsigned char *src, const size_t stride, const color<double>& col)
{
	impl_->set_region(rect, width, height, src, stride, col);
}

size_t texture_atlas_set::width() const { return impl_->width(); }
size_t texture_atlas_set::height() const { return impl_->height(); }
size_t texture_atlas_set::depth() const { return impl_->depth(); }

const uint8_t* texture_atlas_set::data(const int index) const { return impl_->data(index); }
size_t texture_atlas_set::size() const { return impl_->size(); }

}}}
