idf_component_register(
                SRCS 
                  "./expressions_unit_test.cpp"
                  "./jsmnr_test.cpp"
                  "./rdjson_test.cpp"
                  "./ConfigBase_test.cpp"
                  "./rdjson_perf_test.cpp"
                  "./exprCtx_unit_test.cpp"
                INCLUDE_DIRS
                  "."
                REQUIRES
                  unity
                  core
                )

