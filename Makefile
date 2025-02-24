# This file was auto-generated by Polybuild

ifndef OS
	OS := $(shell uname)
endif

obj_ext := .o
ifeq ($(OS),Windows_NT)
	obj_ext := .obj
	out_ext := .exe
endif

compiler := $(CXX)
compilation_flags := -Wall -std=c++17 -O3 -march=native -mtune=native -pthread -Ilibdatachannel/include `pkg-config --cflags gstreamer-video-1.0 gstreamer-app-1.0 gstreamer-1.0`
libraries := -lfltk -lSDL2main -lSDL2 -lssl -lcrypto `pkg-config --libs gstreamer-video-1.0 gstreamer-app-1.0 gstreamer-1.0`
static_libraries := libdatachannel/build/libdatachannel-static.a libdatachannel/build/deps/libjuice/libjuice-static.a libdatachannel/build/deps/libsrtp/libsrtp2.a libdatachannel/build/deps/usrsctp/usrsctplib/libusrsctp.a

ifeq ($(OS),Windows_NT)
	compilation_flags := -Wall -std=c++17 -O3 -march=native -mtune=native -pthread -static-libgcc -static-libstdc++ -mwindows -Ilibdatachannel/include -Llibdatachannel/build `pkg-config --cflags gstreamer-video-1.0 gstreamer-app-1.0 gstreamer-1.0`
	libraries := -lfltk -lSDL2main -lSDL2 -ldatachannel -lssl -lcrypto -lws2_32 -lcomctl32 -lole32 -luuid `pkg-config --libs gstreamer-video-1.0 gstreamer-app-1.0 gstreamer-1.0`
	static_libraries :=
endif

default: lux-desktop$(out_ext)
.PHONY: default

obj/media_receiver_0$(obj_ext): ./media_receiver.cpp ./media_receiver.hpp libdatachannel/include/rtc/rtc.hpp libdatachannel/include/rtc/rtc.h libdatachannel/include/rtc/version.h libdatachannel/include/rtc/common.hpp libdatachannel/include/rtc/utils.hpp libdatachannel/include/rtc/global.hpp libdatachannel/include/rtc/datachannel.hpp libdatachannel/include/rtc/channel.hpp libdatachannel/include/rtc/reliability.hpp libdatachannel/include/rtc/peerconnection.hpp libdatachannel/include/rtc/candidate.hpp libdatachannel/include/rtc/configuration.hpp libdatachannel/include/rtc/description.hpp libdatachannel/include/rtc/track.hpp libdatachannel/include/rtc/mediahandler.hpp libdatachannel/include/rtc/message.hpp libdatachannel/include/rtc/frameinfo.hpp libdatachannel/include/rtc/websocket.hpp libdatachannel/include/rtc/websocketserver.hpp libdatachannel/include/rtc/av1rtppacketizer.hpp libdatachannel/include/rtc/nalunit.hpp libdatachannel/include/rtc/rtppacketizer.hpp libdatachannel/include/rtc/rtppacketizationconfig.hpp libdatachannel/include/rtc/rtp.hpp libdatachannel/include/rtc/h264rtppacketizer.hpp libdatachannel/include/rtc/h264rtpdepacketizer.hpp libdatachannel/include/rtc/h265rtppacketizer.hpp libdatachannel/include/rtc/h265nalunit.hpp libdatachannel/include/rtc/h265rtpdepacketizer.hpp libdatachannel/include/rtc/plihandler.hpp libdatachannel/include/rtc/rembhandler.hpp libdatachannel/include/rtc/pacinghandler.hpp libdatachannel/include/rtc/rtcpnackresponder.hpp libdatachannel/include/rtc/rtcpreceivingsession.hpp libdatachannel/include/rtc/rtcpsrreporter.hpp libdatachannel/include/rtc/rtpdepacketizer.hpp
	@printf '\033[1m[POLYBUILD]\033[0m Compiling $@ from $<...\n'
	@mkdir -p obj
	@$(compiler) -c $< $(compilation_flags) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished compiling $@ from $<!\n'

obj/main_0$(obj_ext): ./main.cpp ./Polyweb/polyweb.hpp ./Polyweb/Polynet/polynet.hpp ./Polyweb/Polynet/string.hpp ./Polyweb/Polynet/secure_sockets.hpp ./Polyweb/Polynet/smart_sockets.hpp ./Polyweb/string.hpp ./Polyweb/threadpool.hpp ./glib.hpp ./json.hpp ./media_receiver.hpp libdatachannel/include/rtc/rtc.hpp libdatachannel/include/rtc/rtc.h libdatachannel/include/rtc/version.h libdatachannel/include/rtc/common.hpp libdatachannel/include/rtc/utils.hpp libdatachannel/include/rtc/global.hpp libdatachannel/include/rtc/datachannel.hpp libdatachannel/include/rtc/channel.hpp libdatachannel/include/rtc/reliability.hpp libdatachannel/include/rtc/peerconnection.hpp libdatachannel/include/rtc/candidate.hpp libdatachannel/include/rtc/configuration.hpp libdatachannel/include/rtc/description.hpp libdatachannel/include/rtc/track.hpp libdatachannel/include/rtc/mediahandler.hpp libdatachannel/include/rtc/message.hpp libdatachannel/include/rtc/frameinfo.hpp libdatachannel/include/rtc/websocket.hpp libdatachannel/include/rtc/websocketserver.hpp libdatachannel/include/rtc/av1rtppacketizer.hpp libdatachannel/include/rtc/nalunit.hpp libdatachannel/include/rtc/rtppacketizer.hpp libdatachannel/include/rtc/rtppacketizationconfig.hpp libdatachannel/include/rtc/rtp.hpp libdatachannel/include/rtc/h264rtppacketizer.hpp libdatachannel/include/rtc/h264rtpdepacketizer.hpp libdatachannel/include/rtc/h265rtppacketizer.hpp libdatachannel/include/rtc/h265nalunit.hpp libdatachannel/include/rtc/h265rtpdepacketizer.hpp libdatachannel/include/rtc/plihandler.hpp libdatachannel/include/rtc/rembhandler.hpp libdatachannel/include/rtc/pacinghandler.hpp libdatachannel/include/rtc/rtcpnackresponder.hpp libdatachannel/include/rtc/rtcpreceivingsession.hpp libdatachannel/include/rtc/rtcpsrreporter.hpp libdatachannel/include/rtc/rtpdepacketizer.hpp ./setup.hpp ./FL_Flex/FL_Flex.H ./video.hpp ./waiter.hpp
	@printf '\033[1m[POLYBUILD]\033[0m Compiling $@ from $<...\n'
	@mkdir -p obj
	@$(compiler) -c $< $(compilation_flags) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished compiling $@ from $<!\n'

obj/video_0$(obj_ext): ./video.cpp ./video.hpp libdatachannel/include/rtc/rtc.hpp libdatachannel/include/rtc/rtc.h libdatachannel/include/rtc/version.h libdatachannel/include/rtc/common.hpp libdatachannel/include/rtc/utils.hpp libdatachannel/include/rtc/global.hpp libdatachannel/include/rtc/datachannel.hpp libdatachannel/include/rtc/channel.hpp libdatachannel/include/rtc/reliability.hpp libdatachannel/include/rtc/peerconnection.hpp libdatachannel/include/rtc/candidate.hpp libdatachannel/include/rtc/configuration.hpp libdatachannel/include/rtc/description.hpp libdatachannel/include/rtc/track.hpp libdatachannel/include/rtc/mediahandler.hpp libdatachannel/include/rtc/message.hpp libdatachannel/include/rtc/frameinfo.hpp libdatachannel/include/rtc/websocket.hpp libdatachannel/include/rtc/websocketserver.hpp libdatachannel/include/rtc/av1rtppacketizer.hpp libdatachannel/include/rtc/nalunit.hpp libdatachannel/include/rtc/rtppacketizer.hpp libdatachannel/include/rtc/rtppacketizationconfig.hpp libdatachannel/include/rtc/rtp.hpp libdatachannel/include/rtc/h264rtppacketizer.hpp libdatachannel/include/rtc/h264rtpdepacketizer.hpp libdatachannel/include/rtc/h265rtppacketizer.hpp libdatachannel/include/rtc/h265nalunit.hpp libdatachannel/include/rtc/h265rtpdepacketizer.hpp libdatachannel/include/rtc/plihandler.hpp libdatachannel/include/rtc/rembhandler.hpp libdatachannel/include/rtc/pacinghandler.hpp libdatachannel/include/rtc/rtcpnackresponder.hpp libdatachannel/include/rtc/rtcpreceivingsession.hpp libdatachannel/include/rtc/rtcpsrreporter.hpp libdatachannel/include/rtc/rtpdepacketizer.hpp ./json.hpp ./keys.hpp
	@printf '\033[1m[POLYBUILD]\033[0m Compiling $@ from $<...\n'
	@mkdir -p obj
	@$(compiler) -c $< $(compilation_flags) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished compiling $@ from $<!\n'

obj/keys_0$(obj_ext): ./keys.cpp ./keys.hpp
	@printf '\033[1m[POLYBUILD]\033[0m Compiling $@ from $<...\n'
	@mkdir -p obj
	@$(compiler) -c $< $(compilation_flags) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished compiling $@ from $<!\n'

obj/setup_0$(obj_ext): ./setup.cpp ./setup.hpp ./FL_Flex/FL_Flex.H ./json.hpp
	@printf '\033[1m[POLYBUILD]\033[0m Compiling $@ from $<...\n'
	@mkdir -p obj
	@$(compiler) -c $< $(compilation_flags) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished compiling $@ from $<!\n'

obj/string_0$(obj_ext): Polyweb/string.cpp Polyweb/string.hpp Polyweb/Polynet/string.hpp
	@printf '\033[1m[POLYBUILD]\033[0m Compiling $@ from $<...\n'
	@mkdir -p obj
	@$(compiler) -c $< $(compilation_flags) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished compiling $@ from $<!\n'

obj/client_0$(obj_ext): Polyweb/client.cpp Polyweb/polyweb.hpp Polyweb/Polynet/polynet.hpp Polyweb/Polynet/string.hpp Polyweb/Polynet/secure_sockets.hpp Polyweb/Polynet/smart_sockets.hpp Polyweb/string.hpp Polyweb/threadpool.hpp
	@printf '\033[1m[POLYBUILD]\033[0m Compiling $@ from $<...\n'
	@mkdir -p obj
	@$(compiler) -c $< $(compilation_flags) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished compiling $@ from $<!\n'

obj/polyweb_0$(obj_ext): Polyweb/polyweb.cpp Polyweb/polyweb.hpp Polyweb/Polynet/polynet.hpp Polyweb/Polynet/string.hpp Polyweb/Polynet/secure_sockets.hpp Polyweb/Polynet/smart_sockets.hpp Polyweb/string.hpp Polyweb/threadpool.hpp
	@printf '\033[1m[POLYBUILD]\033[0m Compiling $@ from $<...\n'
	@mkdir -p obj
	@$(compiler) -c $< $(compilation_flags) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished compiling $@ from $<!\n'

obj/websocket_0$(obj_ext): Polyweb/websocket.cpp Polyweb/polyweb.hpp Polyweb/Polynet/polynet.hpp Polyweb/Polynet/string.hpp Polyweb/Polynet/secure_sockets.hpp Polyweb/Polynet/smart_sockets.hpp Polyweb/string.hpp Polyweb/threadpool.hpp
	@printf '\033[1m[POLYBUILD]\033[0m Compiling $@ from $<...\n'
	@mkdir -p obj
	@$(compiler) -c $< $(compilation_flags) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished compiling $@ from $<!\n'

obj/server_0$(obj_ext): Polyweb/server.cpp Polyweb/polyweb.hpp Polyweb/Polynet/polynet.hpp Polyweb/Polynet/string.hpp Polyweb/Polynet/secure_sockets.hpp Polyweb/Polynet/smart_sockets.hpp Polyweb/string.hpp Polyweb/threadpool.hpp
	@printf '\033[1m[POLYBUILD]\033[0m Compiling $@ from $<...\n'
	@mkdir -p obj
	@$(compiler) -c $< $(compilation_flags) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished compiling $@ from $<!\n'

obj/polynet_0$(obj_ext): Polyweb/Polynet/polynet.cpp Polyweb/Polynet/polynet.hpp Polyweb/Polynet/string.hpp Polyweb/Polynet/secure_sockets.hpp
	@printf '\033[1m[POLYBUILD]\033[0m Compiling $@ from $<...\n'
	@mkdir -p obj
	@$(compiler) -c $< $(compilation_flags) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished compiling $@ from $<!\n'

obj/secure_sockets_0$(obj_ext): Polyweb/Polynet/secure_sockets.cpp Polyweb/Polynet/secure_sockets.hpp Polyweb/Polynet/polynet.hpp Polyweb/Polynet/string.hpp
	@printf '\033[1m[POLYBUILD]\033[0m Compiling $@ from $<...\n'
	@mkdir -p obj
	@$(compiler) -c $< $(compilation_flags) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished compiling $@ from $<!\n'

obj/FL_Flex_0$(obj_ext): FL_Flex/FL_Flex.cpp FL_Flex/FL_Flex.H
	@printf '\033[1m[POLYBUILD]\033[0m Compiling $@ from $<...\n'
	@mkdir -p obj
	@$(compiler) -c $< $(compilation_flags) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished compiling $@ from $<!\n'

lux-desktop$(out_ext): obj/media_receiver_0$(obj_ext) obj/main_0$(obj_ext) obj/video_0$(obj_ext) obj/keys_0$(obj_ext) obj/setup_0$(obj_ext) obj/string_0$(obj_ext) obj/client_0$(obj_ext) obj/polyweb_0$(obj_ext) obj/websocket_0$(obj_ext) obj/server_0$(obj_ext) obj/polynet_0$(obj_ext) obj/secure_sockets_0$(obj_ext) obj/FL_Flex_0$(obj_ext)
	@printf '\033[1m[POLYBUILD]\033[0m Building $@...\n'
	@printf '\033[1m[POLYBUILD]\033[0m Executing prelude: cd libdatachannel && cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release && cd build && $(MAKE) datachannel datachannel-static \n'
	@cd libdatachannel && cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release && cd build && $(MAKE) datachannel datachannel-static 
	@$(compiler) $^ $(static_libraries) $(compilation_flags) $(libraries) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished building $@!\n'

clean:
	@printf '\033[1m[POLYBUILD]\033[0m Executing clean prelude: rm -rf libdatachannel/build\n'
	@rm -rf libdatachannel/build
	@printf '\033[1m[POLYBUILD]\033[0m Deleting lux-desktop$(out_ext) and obj...\n'
	@rm -rf lux-desktop$(out_ext) obj
	@printf '\033[1m[POLYBUILD]\033[0m Finished deleting lux-desktop$(out_ext) and obj!\n'
.PHONY: clean

install:
	@printf '\033[1m[POLYBUILD]\033[0m Copying lux-desktop$(out_ext) to /usr/local/bin...\n'
	@cp lux-desktop$(out_ext) /usr/local/bin
	@printf '\033[1m[POLYBUILD]\033[0m Finished copying lux-desktop to /usr/local/bin!\n'
.PHONY: install
