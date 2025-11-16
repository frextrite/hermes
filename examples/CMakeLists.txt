add_executable(hermes-sync-example
    hermes_sync_example.cpp
)

target_link_libraries(hermes-sync-example
    PRIVATE
        hermes
)

add_executable(hermes-async-example
    hermes_async_example.cpp
)

target_link_libraries(hermes-async-example
    PRIVATE
        hermes
)