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

#include "../StdAfx.h"

#include "layer.h"

#include "frame_producer.h"

#include "../video_format.h"
#include "../frame/audio_channel_layout.h"
#include "../frame/draw_frame.h"
#include "../frame/frame.h"
#include "../frame/frame_transform.h"
#include "../mixer/audio/audio_util.h"

#include <boost/optional.hpp>
#include <boost/thread/future.hpp>

namespace caspar { namespace core {

const_frame get_first_frame_with_audio(draw_frame& frame) {
	struct audio_extractor : public frame_visitor
	{
		std::stack<core::audio_transform> transform_stack_;
		const_frame audio_frame_;
	public:
		audio_extractor()
			: audio_frame_(const_frame::empty())
		{
			transform_stack_.push(audio_transform());
		}

		void push(const frame_transform& transform) override
		{
			transform_stack_.push(transform_stack_.top() * transform.audio_transform);
		}

		void pop() override
		{
			transform_stack_.pop();
		}

		void visit(const const_frame& frame) override
		{
			if (!frame.audio_data().empty() && !transform_stack_.top().is_still && transform_stack_.top().volume > 0.002 && audio_frame_ == const_frame::empty())
				audio_frame_ = frame;
		}
	};

	audio_extractor extractor;
	frame.accept(extractor);

	return extractor.audio_frame_;
}

struct layer::impl
{
	int									index_;
	spl::shared_ptr<monitor::subject>	monitor_subject_;
	tweened_transform					tween_;
	spl::shared_ptr<frame_producer>		foreground_			= frame_producer::empty();
	spl::shared_ptr<frame_producer>		background_			= frame_producer::empty();;
	bool                                auto_play_          = false;
	bool								is_paused_			= false;
	int64_t								current_frame_age_	= 0;

public:
	impl(const int index)
		: index_(index)
		, monitor_subject_(spl::make_shared<monitor::subject>(
				"/layer/" + boost::lexical_cast<std::string>(index)))
//		, foreground_event_subject_("")
//		, background_event_subject_("background")
	{
//		foreground_event_subject_.subscribe(event_subject_);
//		background_event_subject_.subscribe(event_subject_);
	}

	void update_index(const int index)
	{
		index_ = index;
		monitor_subject_->update_path("/layer/" + boost::lexical_cast<std::string>(index));
	}

	void set_foreground(spl::shared_ptr<frame_producer> producer)
	{
		foreground_->monitor_output().detach_parent();
		foreground_ = std::move(producer);
		foreground_->monitor_output().attach_parent(monitor_subject_);
	}

	void pause()
	{
		foreground_->paused(true);
		is_paused_ = true;
	}

	void resume()
	{
		foreground_->paused(false);
		is_paused_ = false;
	}

	void load(spl::shared_ptr<frame_producer> producer, bool preview_producer, bool auto_play)
	{
//		background_->unsubscribe(background_event_subject_);
		background_ = std::move(producer);
//		background_->subscribe(background_event_subject_);

		auto_play_ = auto_play;

		if(preview_producer)
		{
			preview(true);
		}

		if (auto_play_ && foreground_ == frame_producer::empty())
			play();
	}

	void preview(bool force) {
		if (force || background_ != frame_producer::empty()) {
			play();
			receive(video_format_repository::invalid());
			foreground_->paused(true);
			is_paused_ = true;
		}
	}

	void play()
	{
		if(background_ != frame_producer::empty())
		{
			background_->leading_producer(foreground_);

			set_foreground(background_);
			background_ = std::move(frame_producer::empty());

			auto_play_ = false;
		}

		foreground_->paused(false);
		is_paused_ = false;
	}

	void stop()
	{
		set_foreground(frame_producer::empty());

		auto_play_ = false;
	}

        draw_frame receive_background()
        {
            try
            {
                return background_->first_frame();
            }
            catch (...)
            {
                CASPAR_LOG_CURRENT_EXCEPTION();
                background_ = std::move(frame_producer::empty());
                return draw_frame::empty();
            }
        }

	std::pair<draw_frame, draw_frame> receive(const video_format_desc& format_desc)
	{
		try
		{
			*monitor_subject_ << monitor::message("/paused") % is_paused_;

			caspar::timer produce_timer;
			auto frame = foreground_->receive();
			auto produce_time = produce_timer.elapsed();

			*monitor_subject_ << monitor::message("/profiler/time") % produce_time % (1.0 / format_desc.fps);

			if (frame == core::draw_frame::late()) {
				frame = foreground_->last_frame();
			} else {
				if (auto_play_)
				{
					auto auto_play_delta = background_->auto_play_delta();
					if (auto_play_delta)
					{
						auto frames_left = static_cast<int64_t>(foreground_->nb_frames()) - foreground_->frame_number() - static_cast<int64_t>(*auto_play_delta);
						if (frames_left < 1)
						{
							play();
							return receive(format_desc);
						}
					}
				}

				current_frame_age_ = frame.get_and_record_age_millis();
			}

			auto transformed_frame = frame;

			// Apply transform tween to a copy of the frame
			auto transform = tween_.fetch_and_tick(1);
			transformed_frame.transform() *= transform;
			if (format_desc.field_mode != core::field_mode::progressive) {
				auto frame2 = frame;
				frame2.transform() *= tween_.fetch_and_tick(1);
				frame2.transform().audio_transform.volume = 0.0;
				transformed_frame = core::draw_frame::interlace(transformed_frame, frame2, format_desc.field_mode);
			}

			*monitor_subject_ << core::monitor::message("/transform/tween/duration") % tween_.duration()
				<< core::monitor::message("/transform/tween/remaining") % tween_.remaining()
				<< core::monitor::message("/transform/audio/volume") % transform.audio_transform.volume
				<< core::monitor::message("/transform/video/opacity") % transform.image_transform.opacity
				<< core::monitor::message("/transform/video/contrast") % transform.image_transform.contrast
				<< core::monitor::message("/transform/video/brightness") % transform.image_transform.brightness
				<< core::monitor::message("/transform/video/saturation") % transform.image_transform.saturation;

			// TODO - finish properties

			// Layer audio levels
			auto best_audio_frame = get_first_frame_with_audio(frame);
			monitor::subject audio_subject = { "/audio" };
			audio_subject.attach_parent(monitor_subject_);
			auto max_levels = audio_max_level_for_frame(best_audio_frame.audio_channel_layout().num_channels, best_audio_frame.audio_data().data(), best_audio_frame.audio_data().size());
			output_audio_levels(audio_subject, max_levels);

			//event_subject_	<< monitor::event("time")	% monitor::duration(foreground_->frame_number()/format_desc.fps)
			//											% monitor::duration(static_cast<int64_t>(foreground_->nb_frames()) - static_cast<int64_t>(auto_play_delta_ ? *auto_play_delta_ : 0)/format_desc.fps)
			//				<< monitor::event("frame")	% static_cast<int64_t>(foreground_->frame_number())
			//											% static_cast<int64_t>((static_cast<int64_t>(foreground_->nb_frames()) - static_cast<int64_t>(auto_play_delta_ ? *auto_play_delta_ : 0)));

			//foreground_event_subject_ << monitor::event("type") % foreground_->name();
			//background_event_subject_ << monitor::event("type") % background_->name();


			return std::make_pair(frame, transformed_frame);
		}
		catch(...)
		{
			CASPAR_LOG_CURRENT_EXCEPTION();
			stop();
			return std::make_pair(core::draw_frame::empty(), core::draw_frame::empty());
		}
	}

	boost::property_tree::wptree info() const
	{
        auto auto_play_delta = background_->auto_play_delta();

		boost::property_tree::wptree info;
		info.add(L"auto_delta",	(auto_play_delta ? boost::lexical_cast<std::wstring>(*auto_play_delta) : L"null"));
		info.add(L"frame-number", foreground_->frame_number());

		auto nb_frames = foreground_->nb_frames();

		info.add(L"nb_frames",	 nb_frames == std::numeric_limits<int64_t>::max() ? -1 : nb_frames);
		info.add(L"frames-left", nb_frames == std::numeric_limits<int64_t>::max() ? -1 : (foreground_->nb_frames() - foreground_->frame_number() - (auto_play_delta ? *auto_play_delta : 0)));
		info.add(L"frame-age", current_frame_age_);
		info.add_child(L"foreground.producer", foreground_->info());
		info.add_child(L"background.producer", background_->info());

		info.add_child(L"transform", frame_transform_to_tree());

		return info;
	}

	boost::property_tree::wptree frame_transform_to_tree() const {
		auto transform = tween_.fetch();

		boost::property_tree::wptree tween_info;
		tween_info.add(L"duration", tween_.duration());
		tween_info.add(L"remaining", tween_.remaining());

		boost::property_tree::wptree audio_info;
		audio_info.add(L"volume", transform.audio_transform.volume);
		
		boost::property_tree::wptree video_info;
		video_info.add(L"opacity", transform.image_transform.opacity);
		video_info.add(L"contrast", transform.image_transform.contrast);
		video_info.add(L"brightness", transform.image_transform.brightness);
		video_info.add(L"saturation", transform.image_transform.saturation);

		// TODO - finish properties

		boost::property_tree::wptree info;
		info.add_child(L"tween", tween_info);
		info.add_child(L"audio", audio_info);
		info.add_child(L"video", video_info);

		return info;
	}

	boost::property_tree::wptree delay_info() const
	{
		boost::property_tree::wptree info;
		info.add(L"producer", foreground_->print());
		info.add(L"frame-age", current_frame_age_);
		return info;
	}

	void on_interaction(const interaction_event::ptr& event)
	{
		foreground_->on_interaction(event);
	}

	bool collides(double x, double y) const
	{
		return foreground_->collides(x, y);
	}
};


layer::layer(int index) : impl_(new impl(index)){}
layer::layer(layer&& other) : impl_(std::move(other.impl_)){}
layer& layer::operator=(layer&& other)
{
	other.swap(*this);
	return *this;
}
void layer::swap(layer& other)
{
	impl_.swap(other.impl_);

	const int old_index = impl_->index_;
	impl_->update_index(other.impl_->index_);
	other.impl_->update_index(old_index);
}
void layer::load(spl::shared_ptr<frame_producer> frame_producer, bool preview, bool auto_play){return impl_->load(std::move(frame_producer), preview, auto_play);}
void layer::play() { impl_->play(); }
void layer::preview() { impl_->preview(false); }
void layer::pause(){impl_->pause();}
void layer::resume(){impl_->resume();}
void layer::stop(){impl_->stop();}
std::pair<draw_frame, draw_frame> layer::receive(const video_format_desc& format_desc) { return impl_->receive(format_desc); }
draw_frame layer::receive_background() { return impl_->receive_background(); }
spl::shared_ptr<frame_producer> layer::foreground() const { return impl_->foreground_;}
spl::shared_ptr<frame_producer> layer::background() const { return impl_->background_;}
tweened_transform& layer::tween() const { return impl_->tween_; }
void layer::tween(tweened_transform new_tween) { impl_->tween_ = new_tween; }
bool layer::has_background() const { return impl_->background_ != frame_producer::empty(); }
boost::property_tree::wptree layer::info() const{return impl_->info();}
boost::property_tree::wptree layer::delay_info() const{return impl_->delay_info();}
monitor::subject& layer::monitor_output() {return *impl_->monitor_subject_;}
void layer::on_interaction(const interaction_event::ptr& event) { impl_->on_interaction(event); }
bool layer::collides(double x, double y) const { return impl_->collides(x, y); }
}}
