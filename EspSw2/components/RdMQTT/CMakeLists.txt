idf_component_register(
                SRCS 
                  "src/RdMQTTClient.cpp"
                  "src/MQTTProtocol.cpp"
                INCLUDE_DIRS
                  "src"
                REQUIRES
                  core
                )

