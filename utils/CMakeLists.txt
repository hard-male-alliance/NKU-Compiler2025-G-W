# Shared support routines used by both middle-end and backend analyses.
add_library(sysy_utils OBJECT dom_analyser.cpp dynamic_bitset.cpp)
target_link_libraries(sysy_utils PRIVATE sysy_options)
