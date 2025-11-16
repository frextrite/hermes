add_executable(hermes-basic-example
    hermes_basic_example.cpp
)

target_link_libraries(hermes-basic-example
    PRIVATE
        hermes
)