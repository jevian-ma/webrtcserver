var WebrtcServer = function () {
    this.pc = null;
    this.sender = function (url, iceServers, stream, roomid) {
        var _this = this
        return new Promise (function (resolve, reject) {
            _this.roomid = roomid;
            var postdata = {
                act: 'createliveroom',
                roomid: roomid
            }
            if (iceServers) {
                _this.pc = new RTCPeerConnection({iceServers: iceServers})
                var servers = {}
                var turnlist = []
                for (var i = 0, length = iceServers.length ; i < length ; i++) {
                    var iceServer = iceServers[i]
                    var urls = iceServer.urls
                    var arr = urls.split(':')
                    if (arr[0] == 'stun') {
                        servers.stun_server = arr[1]
                        servers.stun_port = Number(arr[2])
                    } else if (arr[0] == 'turn') {
                        var turn = {
                            turn_server: arr[1],
                            turn_port: Number(arr[2])
                        }
                        turn.turn_user = iceServer.username
                        turn.turn_pwd = iceServer.credential
                        turnlist.push(turn)
                    }
                }
                if (turnlist.length != 0) {
                    servers.turnlist = turnlist
                }
                postdata.iceservers = servers
            } else {
                _this.pc = new RTCPeerConnection()
            }
            var videocandidates = []
            var audiocandidates = []
            _this.pc.onicecandidate = function (e) {
                if (e.candidate) {
                    var arr = e.candidate.candidate.split(' ')
                    if (e.candidate.sdpMid == 'video') {
                        var candidate = {
                            priority: Number(arr[3]),
                            ipaddr: arr[4],
                            port: Number(arr[5]),
                            type: arr[7],
                            sdpMLineIndex: e.candidate.sdpMLineIndex,
                            usernameFragment: e.candidate.usernameFragment
                        }
                        videocandidates.push(candidate)
                    } else if (e.candidate.sdpMid == 'audio') {
                        var candidate = {
                            priority: Number(arr[3]),
                            ipaddr: arr[4],
                            port: Number(arr[5]),
                            type: arr[7],
                            sdpMLineIndex: e.candidate.sdpMLineIndex,
                            usernameFragment: e.candidate.usernameFragment
                        }
                        audiocandidates.push(candidate)
                    }
                } else {
                    if (videocandidates.length != 0) {
                        postdata.videoice.candidates = videocandidates
                    }
                    if (audiocandidates.length != 0) {
                        postdata.audioice.candidates = audiocandidates
                    }
                    var xhr = new XMLHttpRequest()
                    xhr.open('POST', url)
                    xhr.onload = function () {
                        var json = xhr.responseText
                        var obj = JSON.parse(json)
                        if (obj.errcode != 0) {
                            alert ('errcode:' + obj.errcode + '; message:' + obj.errmsg)
                            return
                        }
                        var disc = _this.createdisc (obj)
                        _this.pc.setRemoteDescription(disc).catch(function (e) {
                            console.log('set remote desc error')
                            console.log(e)
                        })
                        var ices = _this.createcandidate (obj)
                        for (var i=0,length=ices.length;i<length;i++) {
                            console.log(ices[i])
                            _this.pc.addIceCandidate(ices[i]).catch(function (e) {
                                console.log('add ice error,i=' + i)
                                console.log(e)
                            })
                        }
                        _this.videostreamid = obj.videoice.stream_id
                        _this.audiostreamid = obj.audioice.stream_id
                        resolve(json)
                    }
                    var json = JSON.stringify(postdata)
                    xhr.send(json)
                }
            }
/*
            pc.ontrack = function (e) {
                var clientview = document.getElementById('clientview')
                clientview.srcObject = e.streams[0]
            };
*/
            if (stream) {
                var videotracks = stream.getVideoTracks()
                if (videotracks && videotracks.length > 0) {
                    _this.pc.addTrack(videotracks[0], stream)
                }
                var audiotracks = stream.getAudioTracks()
                if (audiotracks && audiotracks.length > 0) {
                    _this.pc.addTrack(audiotracks[0], stream)
                }
            }
            _this.pc.createOffer({offerToReceiveAudio: true}).then(function (desc) {
                console.log (desc)
                _this.pc.setLocalDescription(desc).catch(function (e) {
                    console.log(e)
                })
                postdata.type = desc.type
                var nowmsg = ''
                var audioiceufrag = ''
                var audioicepwd = ''
                var videoiceufrag = ''
                var videoicepwd = ''
                var audioflagkey = 'm=audio'
                var videoflagkey = 'm=video'
                var iceufragkey = 'a=ice-ufrag:'
                var icepwdkey = 'a=ice-pwd:'
                var arr = desc.sdp.split('\r\n')
                for (var i = 0, length = arr.length ; i < length ; i++) {
                    var tmp = arr[i]
                    if (tmp.substr(0, audioflagkey.length) == audioflagkey) {
                        nowmsg = 'audio'
                    } else if (tmp.substr(0, videoflagkey.length) == videoflagkey) {
                        nowmsg = 'video'
                    } else if (tmp.substr(0, iceufragkey.length) == iceufragkey) {
                        if (nowmsg == 'video') {
                            if (videoiceufrag == '') {
                                videoiceufrag = tmp.substr(iceufragkey.length)
                            }
                        } else if (nowmsg == 'audio') {
                            if (audioiceufrag == '') {
                                audioiceufrag = tmp.substr(iceufragkey.length)
                            }
                        }
                    } else if (tmp.substr(0, icepwdkey.length) == icepwdkey) {
                        if (nowmsg == 'video') {
                            if (videoicepwd == '') {
                                videoicepwd = tmp.substr(icepwdkey.length)
                            }
                        } else if (nowmsg == 'audio') {
                            if (audioicepwd == '') {
                                audioicepwd = tmp.substr(icepwdkey.length)
                            }
                        }
                    }
                }
                postdata.audioice = {
                    iceufrag: audioiceufrag,
                    icepwd: audioicepwd
                }
                postdata.videoice = {
                    iceufrag: videoiceufrag,
                    icepwd: videoicepwd
                }
            })
        })
    }
    this.createdisc = function (obj) {
        var _this = this
        var sdp = 'v=0\r\n' // sdp版本号
        // o=<username> <sess-id> <sess-version> <nettype> <addrtype> <unicast-address>
        // username如何没有使用-代替，3967017503571418851是整个会话的编号，2代表会话版本，如果在会话过程中有改变编码之类的操作，重新生成sdp时,sess-id不变，sess-version加1
        sdp += 'o=- ' + _this.randomnumber(19) + ' 2 IN IP4 127.0.0.1\r\n'
        sdp += 's=-\r\n' // 会话名
        sdp += 't=0 0\r\n' // 会话的起始时间和结束时间，0代表没有限制
        sdp += 'a=group:BUNDLE audio video\r\n' // 表示需要共用一个传输通道传输的媒体，通过ssrc进行区分不同的流。如果没有这一行，音视频数据就会分别用单独udp端口来发送.
        // WMS是WebRTC Media Stream简称;
        // 这一行定义了本客户端支持同时传输多个流，一个流可以包括多个track.
        // 一般定义了这个，后面a=ssrc这一行就会有msid,mslabel等属性.
        sdp += 'a=msid-semantic: WMS ' + _this.randomstring(36) + '\r\n'
        // -------------------------------- 【Stream Description部分】 -------------------------------- 

        // ------------ audio部分 -------------

        // m意味着它是一个媒体行.
        // m=audio说明本会话包含音频，9代表音频使用端口9来传输，但是在webrtc中现在一般不使用，如果设置为0，代表不传输音频,
        // UDP/TLS/RTP/SAVPF是表示用户支持来传输音频的协议，udp,tls,rtp代表使用udp来传输rtp包，并使用tls加密
        // SAVPF代表使用srtcp的反馈机制来控制通信过程
        // 后面的111 103 104 9 0 8 106 105 13 110 112 113 126表示本会话音频支持的编码，后面几行会有详细补充说明.
        sdp += 'm=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 126\r\n'
        // 表示你要用来接收或者发送音频使用的IP地址.
        // webrtc使用ice传输，不使用这个地址
        sdp += 'c=IN IP4 0.0.0.0\r\n'
        // 用来传输rtcp的地址和端口，webrtc中不使用
        sdp += 'a=rtcp:9 IN IP4 0.0.0.0\r\n'
        // 下面2行是ice协商过程中的安全验证信息
        sdp += 'a=ice-ufrag:' + obj.audioice.iceufrag + '\r\n'
        sdp += 'a=ice-pwd:' + obj.audioice.icepwd + '\r\n'
        // 通知对端支持trickle，即sdp里面描述媒体信息和ice候选项的信息可以分开传输
        sdp += 'a=ice-options:trickle\r\n'
        // dtls协商过程中需要的认证信息
        sdp += 'a=fingerprint:sha-256 FC:A4:27:DA:60:12:56:30:88:F4:BC:27:4C:10:BF:AD:8B:D9:82:2D:0D:38:4E:49:26:76:D4:81:AA:70:DD:2A\r\n'
        // 代表本客户端在dtls协商过程中，可以做客户端也可以做服务端, 参考rfc4145 rfc4572
        sdp += 'a=setup:actpass\r\n'
        // 前面BUNDLE行中用到的媒体标识
        sdp += 'a=mid:audio\r\n'
        // 指出要在rtp头部中加入音量信息，参考 rfc6464
        sdp += 'a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\n'
        // 指出是双向通信，另外几种类型是recvonly,sendonly,inactive
        sdp += 'a=sendrecv\r\n'
        // 指出rtp,rtcp包使用同一个端口来传输
        sdp += 'a=rtcp-mux\r\n'
        // 下面十几行都是对m=audio这一行的媒体编码补充说明，指出了编码采用的编号，采样率，声道等
        sdp += 'a=rtpmap:111 opus/48000/2\r\n'
        sdp += 'a=rtcp-fb:111 transport-cc\r\n'
        // 下面一行对opus编码可选的补充说明,minptime代表最小打包时长是10ms，useinbandfec=1代表使用opus编码内置fec特性
        sdp += 'a=fmtp:111 minptime=10;useinbandfec=1\r\n'
        sdp += 'a=rtpmap:103 ISAC/16000\r\n'
        sdp += 'a=rtpmap:104 ISAC/32000\r\n'
        sdp += 'a=rtpmap:9 G722/8000\r\n'
        sdp += 'a=rtpmap:0 PCMU/8000\r\n'
        sdp += 'a=rtpmap:8 PCMA/8000\r\n'
        sdp += 'a=rtpmap:106 CN/32000\r\n'
        sdp += 'a=rtpmap:105 CN/16000\r\n'
        sdp += 'a=rtpmap:13 CN/8000\r\n'
        sdp += 'a=rtpmap:110 telephone-event/48000\r\n'
        sdp += 'a=rtpmap:112 telephone-event/32000\r\n'
        sdp += 'a=rtpmap:113 telephone-event/16000\r\n'
        sdp += 'a=rtpmap:126 telephone-event/8000\r\n'
        sdp += 'a=ssrc:4092501628 cname:JpzPKTUIw3KBild1\r\n'
        sdp += 'a=ssrc:4092501628 msid:eGCad2fmUemi7EMcZtjXeJuNmOiR5rFhOh2q 1f4ab495-c3dd-4ab7-ae35-9f035ccc265e\r\n'
        sdp += 'a=ssrc:4092501628 mslabel:eGCad2fmUemi7EMcZtjXeJuNmOiR5rFhOh2q\r\n'
        sdp += 'a=ssrc:4092501628 label:1f4ab495-c3dd-4ab7-ae35-9f035ccc265e\r\n'

        // ------------ video部分 -------------

        sdp += 'm=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 102 123 127 122 125 107 108 109 124\r\n'
        sdp += 'c=IN IP4 0.0.0.0\r\n'
        sdp += 'a=rtcp:9 IN IP4 0.0.0.0\r\n'
        sdp += 'a=ice-ufrag:' + obj.videoice.iceufrag + '\r\n'
        sdp += 'a=ice-pwd:' + obj.videoice.icepwd + '\r\n'
        sdp += 'a=ice-options:trickle\r\n'
        sdp += 'a=fingerprint:sha-256 FC:A4:27:DA:60:12:56:30:88:F4:BC:27:4C:10:BF:AD:8B:D9:82:2D:0D:38:4E:49:26:76:D4:81:AA:70:DD:2A\r\n'
        sdp += 'a=setup:actpass\r\n'
        sdp += 'a=mid:video\r\n'
        sdp += 'a=extmap:2 urn:ietf:params:rtp-hdrext:toffset\r\n'
        sdp += 'a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\n'
        sdp += 'a=extmap:4 urn:3gpp:video-orientation\r\n'
        sdp += 'a=extmap:5 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01\r\n'
        sdp += 'a=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay\r\n'
        sdp += 'a=extmap:7 http://www.webrtc.org/experiments/rtp-hdrext/video-content-type\r\n'
        sdp += 'a=extmap:8 http://www.webrtc.org/experiments/rtp-hdrext/video-timing\r\n'
        sdp += 'a=sendrecv\r\n'
        sdp += 'a=rtcp-mux\r\n'
        sdp += 'a=rtcp-rsize\r\n'
        sdp += 'a=rtpmap:96 VP8/90000\r\n'
        sdp += 'a=rtcp-fb:96 goog-remb\r\n'
        sdp += 'a=rtcp-fb:96 transport-cc\r\n'
        sdp += 'a=rtcp-fb:96 ccm fir\r\n'
        sdp += 'a=rtcp-fb:96 nack\r\n'
        sdp += 'a=rtcp-fb:96 nack pli\r\n'
        sdp += 'a=rtpmap:97 rtx/90000\r\n'
        sdp += 'a=fmtp:97 apt=96\r\n'
        sdp += 'a=rtpmap:98 VP9/90000\r\n'
        sdp += 'a=rtcp-fb:98 goog-remb\r\n'
        sdp += 'a=rtcp-fb:98 transport-cc\r\n'
        sdp += 'a=rtcp-fb:98 ccm fir\r\n'
        sdp += 'a=rtcp-fb:98 nack\r\n'
        sdp += 'a=rtcp-fb:98 nack pli\r\n'
        sdp += 'a=rtpmap:99 rtx/90000\r\n'
        sdp += 'a=fmtp:99 apt=98\r\n'
        sdp += 'a=rtpmap:100 H264/90000\r\n'
        sdp += 'a=rtcp-fb:100 goog-remb\r\n'
        sdp += 'a=rtcp-fb:100 transport-cc\r\n'
        sdp += 'a=rtcp-fb:100 ccm fir\r\n'
        sdp += 'a=rtcp-fb:100 nack\r\n'
        sdp += 'a=rtcp-fb:100 nack pli\r\n'
        sdp += 'a=fmtp:100 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42001f\r\n'
        sdp += 'a=rtpmap:101 rtx/90000\r\n'
        sdp += 'a=fmtp:101 apt=100\r\n'
        sdp += 'a=rtpmap:102 H264/90000\r\n'
        sdp += 'a=rtcp-fb:102 goog-remb\r\n'
        sdp += 'a=rtcp-fb:102 transport-cc\r\n'
        sdp += 'a=rtcp-fb:102 ccm fir\r\n'
        sdp += 'a=rtcp-fb:102 nack\r\n'
        sdp += 'a=rtcp-fb:102 nack pli\r\n'
        sdp += 'a=fmtp:102 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f\r\n'
        sdp += 'a=rtpmap:123 rtx/90000\r\n'
        sdp += 'a=fmtp:123 apt=102\r\n'
        sdp += 'a=rtpmap:127 H264/90000\r\n'
        sdp += 'a=rtcp-fb:127 goog-remb\r\n'
        sdp += 'a=rtcp-fb:127 transport-cc\r\n'
        sdp += 'a=rtcp-fb:127 ccm fir\r\n'
        sdp += 'a=rtcp-fb:127 nack\r\n'
        sdp += 'a=rtcp-fb:127 nack pli\r\n'
        sdp += 'a=fmtp:127 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=4d0032\r\n'
        sdp += 'a=rtpmap:122 rtx/90000\r\n'
        sdp += 'a=fmtp:122 apt=127\r\n'
        sdp += 'a=rtpmap:125 H264/90000\r\n'
        sdp += 'a=rtcp-fb:125 goog-remb\r\n'
        sdp += 'a=rtcp-fb:125 transport-cc\r\n'
        sdp += 'a=rtcp-fb:125 ccm fir\r\n'
        sdp += 'a=rtcp-fb:125 nack\r\n'
        sdp += 'a=rtcp-fb:125 nack pli\r\n'
        sdp += 'a=fmtp:125 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=640032\r\n'
        sdp += 'a=rtpmap:107 rtx/90000\r\n'
        sdp += 'a=fmtp:107 apt=125\r\n'
        sdp += 'a=rtpmap:109 rtx/90000\r\n'
        sdp += 'a=fmtp:109 apt=108\r\n'
        sdp += 'a=rtpmap:124 ulpfec/90000\r\n'
        sdp += 'a=ssrc-group:FID 255436855 2723160710\r\n'
        sdp += 'a=ssrc:255436855 cname:JpzPKTUIw3KBild1\r\n'
        sdp += 'a=ssrc:255436855 msid:eGCad2fmUemi7EMcZtjXeJuNmOiR5rFhOh2q 9c3b76fb-cfbd-4396-bf35-536e5352763a\r\n'
        sdp += 'a=ssrc:255436855 mslabel:eGCad2fmUemi7EMcZtjXeJuNmOiR5rFhOh2q\r\n'
        sdp += 'a=ssrc:255436855 label:9c3b76fb-cfbd-4396-bf35-536e5352763a\r\n'
        sdp += 'a=ssrc:2723160710 cname:JpzPKTUIw3KBild1\r\n'
        sdp += 'a=ssrc:2723160710 msid:eGCad2fmUemi7EMcZtjXeJuNmOiR5rFhOh2q 9c3b76fb-cfbd-4396-bf35-536e5352763a\r\n'
        sdp += 'a=ssrc:2723160710 mslabel:eGCad2fmUemi7EMcZtjXeJuNmOiR5rFhOh2q\r\n'
        sdp += 'a=ssrc:2723160710 label:9c3b76fb-cfbd-4396-bf35-536e5352763a\r\n'
        var res = {
            sdp: sdp,
            type: 'answer'
        }
        return res
    }
    this.createcandidate = function (obj) {
        var _this = this
        var res = []
        for (var i=0,length=obj.audioice.tracks.length;i<length;i++) {
            var track = obj.videoice.tracks[i]
            res.push({
                candidate: 'candidate:' + _this.randomnumber(10) + ' 1 udp ' + track.priority + ' ' + track.ipaddr + ' ' + track.port + ' typ ' + track.type + ' generation 0 ufrag ' + obj.audioice.iceufrag + ' network-cost 50',
                sdpMid: 'audio',
                sdpMLineIndex: 0,
                usernameFragment: obj.audioice.iceufrag
            })
        }
        for (var i=0,length=obj.videoice.tracks.length;i<length;i++) {
            var track = obj.videoice.tracks[i]
            res.push({
                candidate: 'candidate:' + _this.randomnumber(10) + ' 1 udp ' + track.priority + ' ' + track.ipaddr + ' ' + track.port + ' typ ' + track.type + ' generation 0 ufrag ' + obj.videoice.iceufrag + ' network-cost 50',
                sdpMid: 'video',
                sdpMLineIndex: 1,
                usernameFragment: obj.videoice.iceufrag
            })
        }
        return res
    }
    this.randomnumber = function (len) {
        var num = '';
        for (var i = 0 ; i < len ; i++) {
            num += Math.floor(10 * Math.random());
        }
        return num;
    }
    this.randomstring = function (len) {
        var num = '';
        var arr = ['0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z']
        var length = arr.length
        for (var i = 0 ; i < len ; i++) {
            num += arr[Math.floor(length * Math.random())]
        }
        return num;
    }
}