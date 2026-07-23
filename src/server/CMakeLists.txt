aux_source_directory(. SRC_LIST)
aux_source_directory(./db DB_LIST)
aux_source_directory(./model MODEL_LIST)
aux_source_directory(./redis REDIS_LIST)
aux_source_directory(./config CONFIG_LIST)
aux_source_directory(./util UTIL_LIST)
aux_source_directory(./metrics METRICS_LIST)

add_executable(ChatServer ${SRC_LIST} ${DB_LIST} ${MODEL_LIST} ${REDIS_LIST} ${CONFIG_LIST} ${UTIL_LIST} ${METRICS_LIST})

target_link_libraries(ChatServer mysqlclient muduo_net muduo_base hiredis pthread)
