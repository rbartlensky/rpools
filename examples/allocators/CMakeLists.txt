add_executable(linked_pool_example linked_pool_example.cpp)
# you have to link it with the liblinkedpools.so
target_link_libraries(linked_pool_example linkedpools)

add_executable(linked_pool_example2 linked_pool_example2.cpp)
target_link_libraries(linked_pool_example2 linkedpools)

add_executable(global_linked_pool_example global_linked_pool_example.cpp)
target_link_libraries(global_linked_pool_example linkedpools)
