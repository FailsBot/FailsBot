# bot api catalog. From cpppostbot which factored from the ancient FailsBot.
list (APPEND SOURCE
    bot_easy_api.cpp
    bot_network.cpp
    writefn_data.c
)

list (APPEND HEADERS
    const_str.h
    termcolor.h
    tgtypes.h
    writefn_data.h
)

include_directories(${CURL_INCLUDE_DIR})

add_library(botapi ${SOURCE})