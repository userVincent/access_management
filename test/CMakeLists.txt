idf_component_register(SRCS "main.c"
                            "access/access_test.c"
                            "logger/logger_test.c"
                            "spiffs/spiffs_test.c"
                            "SQL_server/SQL_server_test.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES "certs/servercert.pem"
                                   "certs/prvtkey.pem")

