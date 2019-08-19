/*
* Copyright (c) 2018 Norsk rikskringkasting AS
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
* Author: Julian Waller, julian@superfly.tv
*/

#include "../../StdAfx.h"

#include "sting_producer.h"

#include "../frame_producer.h"
#include "../../frame/draw_frame.h"
#include "../../frame/frame_transform.h"
#include "../../monitor/monitor.h"

#include <tbb/parallel_invoke.h>

#include <future>

namespace caspar {
    namespace core {

        class sting_producer : public frame_producer_base
        {
            spl::shared_ptr<monitor::subject>	monitor_subject_;
            const field_mode					mode_;
            uint32_t									current_frame_ = 0;
            caspar::tweener			audio_tweener_{ L"linear" };

            const sting_info				info_;

            spl::shared_ptr<frame_producer>		dest_producer_;
            spl::shared_ptr<frame_producer>		source_producer_ = frame_producer::empty();
            spl::shared_ptr<frame_producer>               mask_producer_;
            spl::shared_ptr<frame_producer>               overlay_producer_;

            bool								paused_ = false;

        public:
            explicit sting_producer(const field_mode& mode, 
                const spl::shared_ptr<frame_producer>& dest, 
                const sting_info& info, 
                const spl::shared_ptr<frame_producer>& mask,
                const spl::shared_ptr<frame_producer>& overlay)
                : mode_(mode)
                , info_(info)
                , dest_producer_(dest)
                , mask_producer_(mask)
                , overlay_producer_(overlay)
            {
                dest->monitor_output().attach_parent(monitor_subject_);

                CASPAR_LOG(info) << print() << L" Initialized";
            }

            // frame_producer

            void leading_producer(const spl::shared_ptr<frame_producer>& producer) override
            {
                source_producer_ = producer;
            }

            bool is_dest_running() const
            {
                return (current_frame_ >= static_cast<uint32_t>(info_.trigger_point));
            }
            
            draw_frame first_frame() override { 
                return dest_producer_->first_frame(); 
            }

			boost::optional<int64_t> auto_play_delta() const override {
				auto duration = static_cast<int64_t>(mask_producer_->nb_frames());
				// ffmpeg will return -1 when media is still loading, so we need to cast duration first
				if (duration > -1) {
					return boost::optional<int64_t>(duration);
				}
				return boost::none;
			}

			boost::optional<uint32_t> target_duration() const {
				auto autoplay = auto_play_delta();
				if (!autoplay) {
					return boost::none;
				}

				auto autoplay2 = static_cast<uint32_t>(*autoplay);
				if (info_.audio_fade_duration < UINT32_MAX) {
					return std::max(autoplay2, info_.audio_fade_duration + info_.audio_fade_start);
				}
				return autoplay2;
			}

            draw_frame dest_ = draw_frame::empty();
            draw_frame source_ = draw_frame::empty();
            draw_frame mask_ = draw_frame::empty();
            draw_frame overlay_ = draw_frame::empty();

            draw_frame receive_impl() override
            {
                if (has_finished() || mask_producer_ == frame_producer::empty())
                {
                    source_producer_ = frame_producer::empty();
                    mask_producer_ = frame_producer::empty();
                    overlay_producer_ = frame_producer::empty();
                    return dest_producer_->receive();
                }

				bool dest_running = is_dest_running();
				bool expecting_overlay = overlay_producer_ != core::frame_producer::empty();
				tbb::parallel_invoke(
					[&]
				{
					if (source_ == draw_frame::empty()) {
						source_ = source_producer_->receive();
						if (source_ == draw_frame::empty()) {
							source_ = source_producer_->last_frame();
						}
					}
				},
					[&]
				{
					if (dest_ == draw_frame::empty()) {
						dest_ = dest_producer_->receive();
						if (dest_ == draw_frame::empty()) {
							dest_ = dest_producer_->last_frame();
						}
					}
				},
					[&]
				{
					if (mask_ == draw_frame::empty()) {
						mask_ = mask_producer_->receive();
					}
				},
					[&]
				{
					if (expecting_overlay && overlay_ == draw_frame::empty()) {
						overlay_ = overlay_producer_->receive();
					}
				}
				);

				// Not started, and mask or overlay is not ready
				bool mask_and_overlay_valid = mask_ != draw_frame::empty() && (!expecting_overlay || overlay_ != draw_frame::empty());
				if (dest_ == draw_frame::empty() || (current_frame_ == 0 && !mask_and_overlay_valid)) { // Not ready yet
					auto res = source_;
					source_ = draw_frame::empty();
					return res;
				}

				// Ensure mask and overlay are in perfect sync
				auto mask = mask_;
				auto overlay = overlay_;
				if (!mask_and_overlay_valid) {
					// If one is behind, then fetch the last_frame of both
					mask = mask_producer_->last_frame();
					overlay = overlay_producer_->last_frame();
				}

				auto res = compose(dest_running ? dest_ : draw_frame::empty(), source_, mask, overlay);

				if (dest_running) {
					dest_ = draw_frame::empty();
				}
				source_ = draw_frame::empty();

				if (mask_and_overlay_valid) {
					mask_ = draw_frame::empty();
					overlay_ = draw_frame::empty();

					current_frame_ += 1;
				}

				// TODO include info on the mask and overlay producers
				auto duration = target_duration();
				if (duration) {
					*monitor_subject_ << monitor::message("/transition/frame") % static_cast<int>(current_frame_) % static_cast<int>(*duration);
				}

				return res;
            }

            const spl::shared_ptr<frame_producer>& primary_producer() const
            {
                return (is_dest_running() ? dest_producer_ : source_producer_);
            }

            bool has_finished() const 
            {
				auto duration = target_duration();
                return duration && current_frame_ >= *duration;
            }

            draw_frame last_frame() override
            {
                if (has_finished())
                    return dest_producer_->last_frame();

                return frame_producer_base::last_frame();
            }

            constraints& pixel_constraints() override
            {
                return primary_producer()->pixel_constraints();
            }

            uint32_t nb_frames() const override
            {
                return primary_producer()->nb_frames();
            }

            uint32_t frame_number() const override
            {
                return primary_producer()->frame_number();
            }

            std::wstring print() const override
            {
                return L"sting[" + source_producer_->print() + L"=>" + dest_producer_->print() + L"]";
            }

            std::wstring name() const override
            {
                return L"sting";
            }

            boost::property_tree::wptree info() const override
            {
				auto duration = target_duration();

                boost::property_tree::wptree trans_info;
                trans_info.add(L"type", "sting");
                trans_info.add(L"frame", current_frame_);
                trans_info.add(L"duration", duration ? *duration : -1);
                trans_info.add(L"mask_filename", info_.mask_filename);
                trans_info.add(L"overlay_filename", info_.overlay_filename);
                trans_info.add(L"trigger_point", info_.trigger_point);

                auto info = primary_producer()->info();
				info.add_child(L"transition", trans_info);
                return info;
            }

            std::future<std::wstring> call(const std::vector<std::wstring>& params) override
            {
                return primary_producer()->call(params);
            }

            // transition_producer

			double get_audio_delta() const {
				if (current_frame_ < info_.audio_fade_start) {
					return 0;
				}

				auto total_duration = target_duration();
				if (!total_duration) {
					return 0;
				}

				uint32_t frame_number = current_frame_ - info_.audio_fade_start;
				uint32_t duration = std::min(*total_duration - info_.audio_fade_start, info_.audio_fade_duration);
				if (frame_number > duration) {
					return 1.0;
				}

				return audio_tweener_(frame_number, 0.0, 1.0, static_cast<double>(duration));
			}

            draw_frame compose(draw_frame dest_frame, draw_frame src_frame, draw_frame mask_frame, draw_frame overlay_frame) const
            {
				const double delta2 = get_audio_delta();

                src_frame.transform().audio_transform.volume = 1.0 - delta2;
                dest_frame.transform().audio_transform.volume = delta2;

                draw_frame mask_frame2 = mask_frame;
                
                mask_frame.transform().image_transform.is_key = true;
                mask_frame2.transform().image_transform.is_key = true;
                mask_frame2.transform().image_transform.invert = true;

                std::vector<draw_frame> frames;
                frames.push_back(std::move(mask_frame2));
                frames.push_back(std::move(src_frame));
                frames.push_back(std::move(mask_frame));
                frames.push_back(std::move(dest_frame));

                if (overlay_frame != draw_frame::empty())
                    frames.push_back(std::move(overlay_frame));

                return draw_frame(std::move(frames));
            }

            monitor::subject& monitor_output() override
            {
                return *monitor_subject_;
            }

            void on_interaction(const interaction_event::ptr& event) override
            {
                primary_producer()->on_interaction(event);
            }

            bool collides(double x, double y) const override
            {
                return primary_producer()->collides(x, y);
            }

            const frame_timecode& timecode() override { return primary_producer()->timecode(); }
            bool                  has_timecode() override { return primary_producer()->has_timecode(); }
            bool                  provides_timecode() override { return primary_producer()->provides_timecode(); }
        };

        spl::shared_ptr<frame_producer> create_sting_producer(const frame_producer_dependencies& dependencies,
            const field_mode& mode, 
            const spl::shared_ptr<frame_producer>& destination, 
            sting_info& info)
        {
            // Any producer which exposes a fixed duration will work here, not just ffmpeg
            auto mask_producer = dependencies.producer_registry->create_producer(dependencies, info.mask_filename);
            
            auto overlay_producer = frame_producer::empty();
            if (info.overlay_filename != L"") {
                // This could be any producer, no requirement for it to be of fixed length
                overlay_producer = dependencies.producer_registry->create_producer(dependencies, info.overlay_filename);
            }

            return spl::make_shared<sting_producer>(mode, destination, info, mask_producer, overlay_producer);
        }

    }
}
