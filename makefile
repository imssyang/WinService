
MC = mc /U
MT = mt
RC = rc /v
CC = cl /c /nologo
LINK = link /nologo

PROGRAM = winsvc
PROJECT = .
BUILD = $(PROJECT)/build
SOURCE = $(PROJECT)/source

CFLAG = /utf-8 /std:c++17 /EHsc
LFLAG = /machine:X64

!IFDEF DEBUG
CFLAG = $(CFLAG) /Zi /Od /MDd /RTC1
LFLAG = $(LFLAG) /DEBUG
!ELSE
CFLAG = $(CFLAG) /O1
!ENDIF

!IFDEF FREETYPE
CFLAG = $(CFLAG) /D "IMGUI_ENABLE_FREETYPE"
!ENDIF

CFLAG = $(CFLAG) /I $(SOURCE) /I $(SOURCE)/util
CFLAG_GUI = $(CFLAG) /I $(SOURCE)/res
CFLAG_GUI = $(CFLAG) /I $(SOURCE)/gui
CFLAG_GUI = $(CFLAG_GUI) /I $(SOURCE)/gui/imgui
CFLAG_GUI = $(CFLAG_GUI) /I $(SOURCE)/gui/imgui/backends
CFLAG_GUI = $(CFLAG_GUI) /I $(SOURCE)/gui/imgui/misc/freetype
CFLAG_GUI = $(CFLAG_GUI) /I $(SOURCE)/gui/freetype/include
RFLAG_GUI = /I $(SOURCE) /I $(SOURCE)/res /I $(SOURCE)/gui
LFLAG_GUI = $(LFLAG) /SUBSYSTEM:WINDOWS /LIBPATH:"$(SOURCE)/gui/freetype/lib"

{$(SOURCE)\util}.cpp{$(BUILD)\util}.obj:
  mkdir -p $(BUILD)/util
  $(CC) $(CFLAG) /Fo"$(@D)/" $<
util_obj: $(BUILD)/util/*.obj

{$(SOURCE)\core}.cpp{$(BUILD)\core}.obj:
  mkdir -p $(BUILD)/core
  $(CC) $(CFLAG) /Fo"$(@D)/" $<
core_obj: $(BUILD)/core/*.obj

{$(SOURCE)\cmd}.cpp{$(BUILD)\cmd}.obj:
  mkdir -p $(BUILD)/cmd
  $(CC) $(CFLAG) /Fo"$(@D)/" $<
cmd_obj: $(BUILD)/cmd/*.obj

{$(BUILD)\cmd}.obj{$(BUILD)\cmd}.exe:
  $(LINK) $(LFLAG) /out:"$(@D)/$(PROGRAM).exe" \
    $(BUILD)/util/*.obj \
    $(BUILD)/core/*.obj \
    $<
cmd_exe: $(BUILD)/cmd/WSCmd.exe

cmd: util_obj core_obj cmd_obj cmd_exe

{$(SOURCE)\gui\imgui}.cpp{$(BUILD)\gui\imgui}.obj:
  mkdir -p $(BUILD)/gui/imgui/backends
  $(CC) $(CFLAG_GUI) /Fo"$(@D)/" $<
  $(CC) $(CFLAG_GUI) /Fo"$(BUILD)/gui/imgui/backends/" \
    $(SOURCE)/gui/imgui/backends/imgui_impl_win32.cpp \
    $(SOURCE)/gui/imgui/backends/imgui_impl_dx11.cpp
!IFDEF FREETYPE
  mkdir -p $(BUILD)/gui/imgui/misc/freetype
  $(CC) $(CFLAG_GUI) /Fo"$(BUILD)/gui/imgui/misc/freetype/" \
    $(SOURCE)/gui/imgui/misc/freetype/imgui_freetype.cpp
gui_imgui_obj: $(BUILD)/gui/imgui/*.obj $(BUILD)/gui/imgui/backends/*.obj $(BUILD)/gui/imgui/misc/freetype/*.obj
!ELSE
gui_imgui_obj: $(BUILD)/gui/imgui/*.obj $(BUILD)/gui/imgui/backends/*.obj
!ENDIF

{$(SOURCE)\gui\res}.rc{$(BUILD)\gui\res}.res:
  mkdir -p $(BUILD)/gui/res
  $(RC) $(RFLAG_GUI) /Fo"$@" $<
gui_res: $(BUILD)/gui/res/main.res

{$(SOURCE)\gui}.cpp{$(BUILD)\gui}.obj:
  mkdir -p $(BUILD)/gui
  $(CC) $(CFLAG_GUI) /Fo"$(@D)/" $<
gui_obj: $(BUILD)/gui/*.obj

{$(BUILD)\gui}.obj{$(BUILD)\gui}.exe:
  $(LINK) $(LFLAG_GUI) /out:"$(@D)/$(PROGRAM).exe" \
    $(BUILD)/util/*.obj \
    $(BUILD)/core/*.obj \
    $(BUILD)/cmd/*.obj \
    $(BUILD)/gui/imgui/*.obj \
    $(BUILD)/gui/imgui/backends/*.obj \
    $(BUILD)/gui/res/main.res \
!IFDEF FREETYPE
    $(BUILD)/gui/imgui/misc/freetype/*.obj \
    freetype.lib \
!ENDIF
    $<
  $(MT) -manifest $(SOURCE)/gui/res/manifest.xml \
    -outputresource:$(BUILD)/gui/$(PROGRAM).exe;#1

gui_exe: $(BUILD)/gui/WSGui.exe

gui: cmd gui_imgui_obj gui_obj gui_res gui_exe

clean:
  rm -rf $(BUILD)/util \
    $(BUILD)/core \
    $(BUILD)/cmd \
    $(BUILD)/gui \
    $(PROJECT)/log \
    $(PROJECT)/imgui.ini \
    $(PROJECT)/*.pdb
