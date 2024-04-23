
MC = mc /U
RC = rc /v
CC = cl /c /nologo /utf-8 /std:c++17 /O1 /EHsc
LINK = link /nologo /machine:X64

PROGRAM = wsm
PROJECT = .
BUILD = $(PROJECT)/build
SOURCE = $(PROJECT)/source

CFLAG = /I $(SOURCE) /I $(SOURCE)/util
CFLAG_GUI = $(CFLAG) /I $(SOURCE)/res
CFLAG_GUI = $(CFLAG) /I $(SOURCE)/gui
CFLAG_GUI = $(CFLAG_GUI) /I $(SOURCE)/gui/imgui
CFLAG_GUI = $(CFLAG_GUI) /I $(SOURCE)/gui/imgui/backends
LFLAG =
LFLAG_GUI = $(LFLAG) /SUBSYSTEM:WINDOWS

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
cmd_exe: $(BUILD)/cmd/WsmCmd.exe

cmd: util_obj core_obj cmd_obj cmd_exe

{$(SOURCE)\gui\imgui}.cpp{$(BUILD)\gui\imgui}.obj:
  mkdir -p $(BUILD)/gui/imgui/backends
  $(CC) $(CFLAG_GUI) /Fo"$(@D)/" $<
  $(CC) $(CFLAG_GUI) /Fo"$(BUILD)/gui/imgui/backends/" \
    $(SOURCE)/gui/imgui/backends/imgui_impl_win32.cpp \
    $(SOURCE)/gui/imgui/backends/imgui_impl_dx11.cpp
gui_imgui_obj: $(BUILD)/gui/imgui/*.obj $(BUILD)/gui/imgui/backends/*.obj

{$(SOURCE)\gui\res}.rc{$(BUILD)\gui\res}.res:
  mkdir -p $(BUILD)/gui/res
  $(RC) $(CFLAG_GUI) /Fo"$@" $<
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
    $<
gui_exe: $(BUILD)/gui/WsmGui.exe

gui: cmd gui_imgui_obj gui_obj gui_res gui_exe

clean:
  rm -rf $(BUILD)/util \
    $(BUILD)/core \
    $(BUILD)/cmd \
    $(BUILD)/gui \
    $(PROJECT)/log \
    $(PROJECT)/child \
    $(PROJECT)/imgui.ini
