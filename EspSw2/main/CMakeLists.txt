idf_component_register(
                SRCS 
                    "main.cpp"
                    "PiCoProcessor/PiCoProcessor.cpp"
                INCLUDE_DIRS 
                    "."
                    "PiCoProcessor"
                )

# Create a SPIFFS image from the contents of the 'spiffs_image' directory
# that fits the partition named 'spiffs'. FLASH_IN_PROJECT indicates that
# the generated image should be flashed when the entire project is flashed to
# the target with 'idf.py -p PORT flash'. 
spiffs_create_partition_image(spiffs ../spiffs_image FLASH_IN_PROJECT)
