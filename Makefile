# Project Name
TARGET = takshaka

# Sources
CPP_SOURCES = takshaka.cpp BlockSuperSawOsc.cpp SmartKnob.cpp BlockComb.cpp BlockOscillator.cpp BlockSvf.cpp BlockAtone.cpp BlockChorus.cpp BlockReverbSc.cpp BlockFlanger.cpp

# Library Locations
LIBDAISY_DIR = ../../libDaisy/
DAISYSP_DIR = ../../DaisySP/

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
