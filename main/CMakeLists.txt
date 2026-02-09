idf_component_register(SRCS "jive_stick.c"
                    INCLUDE_DIRS ".")

# Flash audio files to the storage partition
# littlefs_create_partition_image(storage ../audio FLASH_IN_PROJECT)