# RDSYSTEM_ALL_LIBRARIES := $(patsubst $(COMPONENT_PATH)/libraries/%,%,$(wildcard $(COMPONENT_PATH)/libraries/*))

# # Macro returns non-empty if RdSystem library $(1) should be included in the build
# # (either because selective compilation is of, or this library is enabled
# define RDSYSTEM_LIBRARY_ENABLED
# $(if $(CONFIG_RDSYSTEM_SELECTIVE_COMPILATION),$(CONFIG_RDSYSTEM_SELECTIVE_$(1)),y)
# endef

# RDSYSTEM_ENABLED_LIBRARIES := $(foreach LIBRARY,$(sort $(RDSYSTEM_ALL_LIBRARIES)),$(if $(call RDSYSTEM_LIBRARY_ENABLED,$(LIBRARY)),$(LIBRARY)))

# $(info RdSystem libraries in build: $(RDSYSTEM_ENABLED_LIBRARIES))

# # Expand all subdirs under $(1)
# define EXPAND_SUBDIRS
# $(sort $(dir $(wildcard $(1)/* $(1)/*/* $(1)/*/*/* $(1)/*/*/*/* $(1)/*/*/*/*/*)))
# endef

# # Macro returns SRCDIRS for library
# define RDSYSTEM_LIBRARY_GET_SRCDIRS
# 	$(if $(wildcard $(COMPONENT_PATH)/libraries/$(1)/src/.), 							\
# 		$(call EXPAND_SUBDIRS,$(COMPONENT_PATH)/libraries/$(1)/src), 					\
# 		$(filter-out $(call EXPAND_SUBDIRS,$(COMPONENT_PATH)/libraries/$(1)/examples),  \
# 			$(call EXPAND_SUBDIRS,$(COMPONENT_PATH)/libraries/$(1)) 				   	\
# 		) 																				\
# 	)
# endef

# # Make a list of all srcdirs in enabled libraries
# RDSYSTEM_LIBRARY_SRCDIRS := $(patsubst $(COMPONENT_PATH)/%,%,$(foreach LIBRARY,$(RDSYSTEM_ENABLED_LIBRARIES),$(call RDSYSTEM_LIBRARY_GET_SRCDIRS,$(LIBRARY))))

# #$(info RdSystem libraries src dirs: $(RDSYSTEM_LIBRARY_SRCDIRS))

# COMPONENT_ADD_INCLUDEDIRS := cores/esp32 variants/esp32 $(RDSYSTEM_LIBRARY_SRCDIRS)
# COMPONENT_PRIV_INCLUDEDIRS := cores/esp32/libb64
# COMPONENT_SRCDIRS := cores/esp32/libb64 cores/esp32 variants/esp32 $(RDSYSTEM_LIBRARY_SRCDIRS)
# CXXFLAGS += -fno-rtti
