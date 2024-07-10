# This file was auto-generated by Polybuild

compiler := $(CXX)
compilation_flags := -Wall -Wno-unused-command-line-argument -std=c++17 -O3 `pkg-config --cflags --libs gtk+-3.0` `pkg-config --cflags --libs gstreamer-1.0` -Ilibdatachannel/include -Llibdatachannel/build
libraries := -ldatachannel -lssl -lcrypto

default: lux-desktop
.PHONY: default

obj/main_0.o: ./main.cpp ./Polyweb/polyweb.hpp ./Polyweb/Polynet/polynet.hpp ./Polyweb/Polynet/secure_sockets.hpp ./Polyweb/Polynet/smart_sockets.hpp ./Polyweb/string.hpp ./Polyweb/threadpool.hpp ./glib.hpp ./json.hpp libdatachannel/include/rtc/rtc.hpp libdatachannel/include/rtc/rtc.h libdatachannel/include/rtc/version.h libdatachannel/include/rtc/common.hpp libdatachannel/include/rtc/utils.hpp libdatachannel/include/rtc/global.hpp libdatachannel/include/rtc/datachannel.hpp libdatachannel/include/rtc/channel.hpp libdatachannel/include/rtc/reliability.hpp libdatachannel/include/rtc/peerconnection.hpp libdatachannel/include/rtc/candidate.hpp libdatachannel/include/rtc/configuration.hpp libdatachannel/include/rtc/description.hpp libdatachannel/include/rtc/track.hpp libdatachannel/include/rtc/mediahandler.hpp libdatachannel/include/rtc/message.hpp libdatachannel/include/rtc/frameinfo.hpp libdatachannel/include/rtc/websocket.hpp libdatachannel/include/rtc/websocketserver.hpp libdatachannel/include/rtc/av1rtppacketizer.hpp libdatachannel/include/rtc/nalunit.hpp libdatachannel/include/rtc/rtppacketizer.hpp libdatachannel/include/rtc/rtppacketizationconfig.hpp libdatachannel/include/rtc/rtp.hpp libdatachannel/include/rtc/h264rtppacketizer.hpp libdatachannel/include/rtc/h264rtpdepacketizer.hpp libdatachannel/include/rtc/h265rtppacketizer.hpp libdatachannel/include/rtc/h265nalunit.hpp libdatachannel/include/rtc/plihandler.hpp libdatachannel/include/rtc/rembhandler.hpp libdatachannel/include/rtc/pacinghandler.hpp libdatachannel/include/rtc/rtcpnackresponder.hpp libdatachannel/include/rtc/rtcpreceivingsession.hpp libdatachannel/include/rtc/rtcpsrreporter.hpp libdatachannel/include/rtc/rtpdepacketizer.hpp
	@printf '\033[1m[POLYBUILD]\033[0m Compiling $@ from $<...\n'
	@mkdir -p obj
	@$(compiler) -c $< $(compilation_flags) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished compiling $@ from $<!\n'

obj/string_0.o: Polyweb/string.cpp Polyweb/string.hpp
	@printf '\033[1m[POLYBUILD]\033[0m Compiling $@ from $<...\n'
	@mkdir -p obj
	@$(compiler) -c $< $(compilation_flags) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished compiling $@ from $<!\n'

obj/client_0.o: Polyweb/client.cpp Polyweb/polyweb.hpp Polyweb/Polynet/polynet.hpp Polyweb/Polynet/secure_sockets.hpp Polyweb/Polynet/smart_sockets.hpp Polyweb/string.hpp Polyweb/threadpool.hpp
	@printf '\033[1m[POLYBUILD]\033[0m Compiling $@ from $<...\n'
	@mkdir -p obj
	@$(compiler) -c $< $(compilation_flags) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished compiling $@ from $<!\n'

obj/polyweb_0.o: Polyweb/polyweb.cpp Polyweb/polyweb.hpp Polyweb/Polynet/polynet.hpp Polyweb/Polynet/secure_sockets.hpp Polyweb/Polynet/smart_sockets.hpp Polyweb/string.hpp Polyweb/threadpool.hpp
	@printf '\033[1m[POLYBUILD]\033[0m Compiling $@ from $<...\n'
	@mkdir -p obj
	@$(compiler) -c $< $(compilation_flags) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished compiling $@ from $<!\n'

obj/websocket_0.o: Polyweb/websocket.cpp Polyweb/polyweb.hpp Polyweb/Polynet/polynet.hpp Polyweb/Polynet/secure_sockets.hpp Polyweb/Polynet/smart_sockets.hpp Polyweb/string.hpp Polyweb/threadpool.hpp
	@printf '\033[1m[POLYBUILD]\033[0m Compiling $@ from $<...\n'
	@mkdir -p obj
	@$(compiler) -c $< $(compilation_flags) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished compiling $@ from $<!\n'

obj/server_0.o: Polyweb/server.cpp Polyweb/polyweb.hpp Polyweb/Polynet/polynet.hpp Polyweb/Polynet/secure_sockets.hpp Polyweb/Polynet/smart_sockets.hpp Polyweb/string.hpp Polyweb/threadpool.hpp
	@printf '\033[1m[POLYBUILD]\033[0m Compiling $@ from $<...\n'
	@mkdir -p obj
	@$(compiler) -c $< $(compilation_flags) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished compiling $@ from $<!\n'

obj/polynet_0.o: Polyweb/Polynet/polynet.cpp Polyweb/Polynet/polynet.hpp Polyweb/Polynet/secure_sockets.hpp
	@printf '\033[1m[POLYBUILD]\033[0m Compiling $@ from $<...\n'
	@mkdir -p obj
	@$(compiler) -c $< $(compilation_flags) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished compiling $@ from $<!\n'

obj/secure_sockets_0.o: Polyweb/Polynet/secure_sockets.cpp Polyweb/Polynet/secure_sockets.hpp Polyweb/Polynet/polynet.hpp
	@printf '\033[1m[POLYBUILD]\033[0m Compiling $@ from $<...\n'
	@mkdir -p obj
	@$(compiler) -c $< $(compilation_flags) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished compiling $@ from $<!\n'

lux-desktop: obj/main_0.o obj/string_0.o obj/client_0.o obj/polyweb_0.o obj/websocket_0.o obj/server_0.o obj/polynet_0.o obj/secure_sockets_0.o
	@printf '\033[1m[POLYBUILD]\033[0m Building $@...\n'
	@$(compiler) $^ $(compilation_flags) $(libraries) -o $@
	@printf '\033[1m[POLYBUILD]\033[0m Finished building $@!\n'

clean:
	@printf '\033[1m[POLYBUILD]\033[0m Deleting lux-desktop and obj...\n'
	@rm -rf lux-desktop obj
	@printf '\033[1m[POLYBUILD]\033[0m Finished deleting lux-desktop and obj!\n'
.PHONY: clean

install:
	@printf '\033[1m[POLYBUILD]\033[0m Copying lux-desktop to /usr/local/bin...\n'
	@cp lux-desktop /usr/local/bin
	@printf '\033[1m[POLYBUILD]\033[0m Finished copying lux-desktop to /usr/local/bin!\n'
.PHONY: install
