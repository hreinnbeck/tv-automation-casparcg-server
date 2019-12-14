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

#include "StdAfx.h"

#include "video_format.h"

#include <boost/algorithm/string.hpp>

namespace caspar { namespace core {

struct video_format_repository::impl
{
private:
	std::map<std::wstring, video_format_desc> formats_;

public:
	impl(): formats_()
	{
		const std::vector<video_format_desc> default_formats = {
			{ video_format::pal,          720,  576,  1024, 576,  field_mode::upper,       25000, 1000, L"PAL",{ 1920 } },
			{ video_format::ntsc,         720,  486,  720,  540,  field_mode::lower,       30000, 1001, L"NTSC",{ 1602, 1601, 1602, 1601, 1602 } },
			{ video_format::x576p2500,    720,  576,  1024, 576,  field_mode::progressive, 25000, 1000, L"576p2500",{ 1920 } },
			{ video_format::x720p2398,    1280, 720,  1280, 720,  field_mode::progressive, 24000, 1001, L"720p2398",{ 2002 } },
			{ video_format::x720p2400,    1280, 720,  1280, 720,  field_mode::progressive, 24000, 1000, L"720p2400",{ 2000 } },
			{ video_format::x720p2500,    1280, 720,  1280, 720,  field_mode::progressive, 25000, 1000, L"720p2500",{ 1920 } },
			{ video_format::x720p5000,    1280, 720,  1280, 720,  field_mode::progressive, 50000, 1000, L"720p5000",{ 960 } },
			{ video_format::x720p2997,    1280, 720,  1280, 720,  field_mode::progressive, 30000, 1001, L"720p2997",{ 1602, 1601, 1602, 1601, 1602 } },
			{ video_format::x720p5994,    1280, 720,  1280, 720,  field_mode::progressive, 60000, 1001, L"720p5994",{ 801,  800,  801,  801,  801 } },
			{ video_format::x720p3000,    1280, 720,  1280, 720,  field_mode::progressive, 30000, 1000, L"720p3000",{ 1600 } },
			{ video_format::x720p6000,    1280, 720,  1280, 720,  field_mode::progressive, 60000, 1000, L"720p6000",{ 800 } },
			{ video_format::x1080p2398,   1920, 1080, 1920, 1080, field_mode::progressive, 24000, 1001, L"1080p2398",{ 2002 } },
			{ video_format::x1080p2400,   1920, 1080, 1920, 1080, field_mode::progressive, 24000, 1000, L"1080p2400",{ 2000 } },
			{ video_format::x1080i5000,   1920, 1080, 1920, 1080, field_mode::upper,       25000, 1000, L"1080i5000",{ 1920 } },
			{ video_format::x1080i5994,   1920, 1080, 1920, 1080, field_mode::upper,       30000, 1001, L"1080i5994",{ 1602, 1601, 1602, 1601, 1602 } },
			{ video_format::x1080i6000,   1920, 1080, 1920, 1080, field_mode::upper,       30000, 1000, L"1080i6000",{ 1600 } },
			{ video_format::x1080p2500,   1920, 1080, 1920, 1080, field_mode::progressive, 25000, 1000, L"1080p2500",{ 1920 } },
			{ video_format::x1080p2997,   1920, 1080, 1920, 1080, field_mode::progressive, 30000, 1001, L"1080p2997",{ 1602, 1601, 1602, 1601, 1602 } },
			{ video_format::x1080p3000,   1920, 1080, 1920, 1080, field_mode::progressive, 30000, 1000, L"1080p3000",{ 1600 } },
			{ video_format::x1080p5000,   1920, 1080, 1920, 1080, field_mode::progressive, 50000, 1000, L"1080p5000",{ 960 } },
			{ video_format::x1080p5994,   1920, 1080, 1920, 1080, field_mode::progressive, 60000, 1001, L"1080p5994",{ 801,  800,  801,  801,  801 } },
			{ video_format::x1080p6000,   1920, 1080, 1920, 1080, field_mode::progressive, 60000, 1000, L"1080p6000",{ 800 } },
			{ video_format::x1556p2398,   2048, 1556, 2048, 1556, field_mode::progressive, 24000, 1001, L"1556p2398",{ 2002 } },
			{ video_format::x1556p2400,   2048, 1556, 2048, 1556, field_mode::progressive, 24000, 1000, L"1556p2400",{ 2000 } },
			{ video_format::x1556p2500,   2048, 1556, 2048, 1556, field_mode::progressive, 25000, 1000, L"1556p2500",{ 1920 } },
			{ video_format::dci1080p2398, 2048, 1080, 2048, 1080, field_mode::progressive, 24000, 1001, L"dci1080p2398",{ 2002 } },
			{ video_format::dci1080p2400, 2048, 1080, 2048, 1080, field_mode::progressive, 24000, 1000, L"dci1080p2400",{ 2000 } },
			{ video_format::dci1080p2500, 2048, 1080, 2048, 1080, field_mode::progressive, 25000, 1000, L"dci1080p2500",{ 1920 } },
			{ video_format::x2160p2398,   3840, 2160, 3840, 2160, field_mode::progressive, 24000, 1001, L"2160p2398",{ 2002 } },
			{ video_format::x2160p2400,   3840, 2160, 3840, 2160, field_mode::progressive, 24000, 1000, L"2160p2400",{ 2000 } },
			{ video_format::x2160p2500,   3840, 2160, 3840, 2160, field_mode::progressive, 25000, 1000, L"2160p2500",{ 1920 } },
			{ video_format::x2160p2997,   3840, 2160, 3840, 2160, field_mode::progressive, 30000, 1001, L"2160p2997",{ 1602, 1601, 1602, 1601, 1602 } },
			{ video_format::x2160p3000,   3840, 2160, 3840, 2160, field_mode::progressive, 30000, 1000, L"2160p3000",{ 1600 } },
			{ video_format::x2160p5000,   3840, 2160, 3840, 2160, field_mode::progressive, 50000, 1000, L"2160p5000",{ 960 } },
			{ video_format::x2160p5994,   3840, 2160, 3840, 2160, field_mode::progressive, 60000, 1001, L"2160p5994",{ 801,  800,  801,  801,  801 } },
			{ video_format::x2160p6000,   3840, 2160, 3840, 2160, field_mode::progressive, 60000, 1000, L"2160p6000",{ 800 } },
			{ video_format::dci2160p2398, 4096, 2160, 4096, 2160, field_mode::progressive, 24000, 1001, L"dci2160p2398",{ 2002 } },
			{ video_format::dci2160p2400, 4096, 2160, 4096, 2160, field_mode::progressive, 24000, 1000, L"dci2160p2400",{ 2000 } },
			{ video_format::dci2160p2500, 4096, 2160, 4096, 2160, field_mode::progressive, 25000, 1000, L"dci2160p2500",{ 1920 } },
		};

		for (auto& f : default_formats)
			formats_.insert({ (boost::to_lower_copy(f.name)), f });
	}

	video_format_desc find(const std::wstring& name) const
	{
		const std::wstring lower = boost::to_lower_copy(name);

		const auto res = formats_.find(lower);
		if (res != formats_.end())
			return res->second;

		return invalid();
	}

	video_format_desc find_format(const video_format& id) const
	{
		for (auto& f: formats_)
		{
			if (f.second.format == id)
				return f.second;
		}

		return invalid();
	}

	void store(const video_format_desc& format)
	{
		const std::wstring lower = boost::to_lower_copy(format.name);
		formats_.insert({ lower, format });
	}


	std::vector<int> find_audio_cadence(const boost::rational<int>& framerate, const bool log_quiet) const
	{
		std::map<boost::rational<int>, std::vector<int>> cadences_by_framerate;

		for (const auto& f: formats_)
		{
			boost::rational<int> format_rate(f.second.time_scale, f.second.duration);

			cadences_by_framerate.insert(std::make_pair(format_rate, f.second.audio_cadence));
		}

		const auto exact_match = cadences_by_framerate.find(framerate);

		if (exact_match != cadences_by_framerate.end())
			return exact_match->second;

		boost::rational<int> closest_framerate_diff = std::numeric_limits<int>::max();
		boost::rational<int> closest_framerate = 0;

		for (const auto format_framerate : cadences_by_framerate | boost::adaptors::map_keys)
		{
			const auto diff = boost::abs(framerate - format_framerate);

			if (diff < closest_framerate_diff)
			{
				closest_framerate_diff = diff;
				closest_framerate = format_framerate;
			}
		}

		if (log_quiet)
			CASPAR_LOG(debug) << "No exact audio cadence match found for framerate " << to_string(framerate)
			<< "\nClosest match is " << to_string(closest_framerate)
			<< "\nwhich is a " << to_string(closest_framerate_diff) << " difference.";
		else
			CASPAR_LOG(warning) << "No exact audio cadence match found for framerate " << to_string(framerate)
			<< "\nClosest match is " << to_string(closest_framerate)
			<< "\nwhich is a " << to_string(closest_framerate_diff) << " difference.";

		return cadences_by_framerate[closest_framerate];
	}

	std::size_t get_max_video_format_size() const
	{
		size_t max = 0;
		for (auto& f : formats_)
		{
			if (f.second.size > max)
				max = f.second.size;
		}

		return max;
	}
};

video_format_repository::video_format_repository() : impl_(new impl()) { }
video_format_desc video_format_repository::invalid() { return video_format_desc(video_format::invalid, 0, 0, 0, 0, field_mode::progressive, 1, 1, L"invalid", { 1 } ); };
video_format_desc video_format_repository::find(const std::wstring& name) const { return impl_->find(name); }
video_format_desc video_format_repository::find_format(const video_format& format) const { return impl_->find_format(format); }
void video_format_repository::store(const video_format_desc& format) { impl_->store(format); }
std::vector<int> video_format_repository::find_audio_cadence(const boost::rational<int>& framerate, const bool log_quiet) const { return impl_->find_audio_cadence(framerate, log_quiet); }
std::size_t video_format_repository::get_max_video_format_size() const { return impl_->get_max_video_format_size(); }

video_format_desc::video_format_desc(
		const video_format format,
		const int width,
		const int height,
		const int square_width,
		const int square_height,
		const core::field_mode field_mode,
		const int time_scale,
		const int duration,
		const std::wstring& name,
		const std::vector<int>& audio_cadence)
	: format(format)
	, width(width)
	, height(height)
	, square_width(square_width)
	, square_height(square_height)
	, field_mode(field_mode)
	, fps(static_cast<double>(time_scale) / static_cast<double>(duration))
	, framerate(time_scale, duration)
	, time_scale(time_scale)
	, duration(duration)
	, field_count(field_mode == core::field_mode::progressive ? 1 : 2)
	, size(width*height*4)
	, name(name)
	, audio_sample_rate(48000)
	, audio_cadence(audio_cadence)
{
}


video_format_desc::video_format_desc()
	: format(video_format::invalid)
	, field_mode(core::field_mode::empty)
{
	*this = video_format_repository::invalid();
}

bool operator==(const video_format_desc& lhs, const video_format_desc& rhs)
{
	// TODO - expand on this if format is custom
	return lhs.format == rhs.format;
}

bool operator!=(const video_format_desc& lhs, const video_format_desc& rhs)
{
	return !(lhs == rhs);
}

std::wostream& operator<<(std::wostream& out, const video_format_desc& format_desc)
{
	out << format_desc.name.c_str();
	return out;
}



}}
