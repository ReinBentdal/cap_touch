
if(CONFIG_DEBUG)
    target_sources(app PRIVATE 
        bt_connection_manager.c
        bt_log.c
    )
endif()