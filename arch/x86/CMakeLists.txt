PROJECT(x86)

ADD_LIBRARY(x86 STATIC
			x86_arch.cpp
			x86_decode.cpp
			x86_disasm.cpp
			x86_disasm_att.cpp
			x86_disasm_intel.cpp
			x86_tag.cpp
			x86_translate.cpp)
