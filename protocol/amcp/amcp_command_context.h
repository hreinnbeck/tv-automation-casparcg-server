#pragma once

#include "../util/ClientInfo.h"
#include "AMCPCommandScheduler.h"
#include "amcp_command_repository.h"
#include "amcp_shared.h"
#include <core/consumer/frame_consumer.h>
#include <future>

namespace caspar { namespace protocol { namespace amcp {

struct amcp_command_static_context
{
    const std::shared_ptr<core::thumbnail_generator>             thumb_gen;
    const spl::shared_ptr<core::media_info_repository>           media_info_repo;
    const spl::shared_ptr<core::system_info_provider_repository> system_info_provider_repo;
	const core::video_format_repository							 format_repository;
    const spl::shared_ptr<core::cg_producer_registry>            cg_registry;
    const spl::shared_ptr<core::help_repository>                 help_repo;
    const spl::shared_ptr<const core::frame_producer_registry>   producer_registry;
    const spl::shared_ptr<const core::frame_consumer_registry>   consumer_registry;
    const spl::shared_ptr<AMCPCommandScheduler>                  scheduler;
    const std::shared_ptr<amcp_command_repository>               parser;
    const std::shared_ptr<accelerator::ogl::device>              ogl_device;
    std::promise<bool>&                                          shutdown_server_now;

    amcp_command_static_context(const std::shared_ptr<core::thumbnail_generator>&             thumb_gen,
                                const spl::shared_ptr<core::media_info_repository>&           media_info_repo,
                                const spl::shared_ptr<core::system_info_provider_repository>& system_info_provider_repo,
                                const core::video_format_repository&                          format_repository,
                                const spl::shared_ptr<core::cg_producer_registry>             cg_registry,
                                const spl::shared_ptr<core::help_repository>&                 help_repo,
                                const spl::shared_ptr<const core::frame_producer_registry>    producer_registry,
                                const spl::shared_ptr<const core::frame_consumer_registry>    consumer_registry,
                                const spl::shared_ptr<AMCPCommandScheduler>                   scheduler,
                                const std::shared_ptr<amcp_command_repository>                parser,
                                const std::shared_ptr<accelerator::ogl::device>&              ogl_device,
                                std::promise<bool>&                                           shutdown_server_now)
        : thumb_gen(std::move(thumb_gen))
        , media_info_repo(std::move(media_info_repo))
        , system_info_provider_repo(std::move(system_info_provider_repo))
		, format_repository(format_repository)
        , cg_registry(std::move(cg_registry))
        , help_repo(std::move(help_repo))
        , producer_registry(std::move(producer_registry))
        , consumer_registry(std::move(consumer_registry))
        , scheduler(std::move(scheduler))
        , parser(std::move(parser))
        , ogl_device(std::move(ogl_device))
        , shutdown_server_now(shutdown_server_now)
    {
    }
};

struct command_context {
    std::shared_ptr<amcp_command_static_context> static_context;
    const std::vector<channel_context>           channels;
    const IO::ClientInfoPtr                      client;
    const channel_context                        channel;
    const int                                    channel_index;
    const int                                    layer_id;
    std::vector<std::wstring>                    parameters;

    int layer_index(int default_ = 0) const { return layer_id == -1 ? default_ : layer_id; }

    command_context(std::shared_ptr<amcp_command_static_context> static_context,
                    const std::vector<channel_context>           channels,
                    IO::ClientInfoPtr                            client,
                    channel_context                              channel,
                    int                                          channel_index,
                    int                                          layer_id)
        : static_context(static_context)
        , channels(std::move(channels))
        , client(std::move(client))
        , channel(channel)
        , channel_index(channel_index)
        , layer_id(layer_id) {
    }
};

}}} // namespace caspar::protocol::amcp
