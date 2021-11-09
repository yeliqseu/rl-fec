def build(bld):
    obj = bld.create_ns3_program('rlfec-stream-server-client-sim',['core', 'point-to-point', 'internet', 'applications','point-to-point-layout','flow-monitor'])
    obj.source= [ 'rlfec-stream-server-client-sim.cc',
				  'rlfec-stream-server-app.cc',
				  'stream-client-app.cc',
				  'handlepacket.cc',
                  'infoqueue.cc',
                  'qlearning.cc',
                  'tiles.cc',
				]
    # Link to libstreamc.a
    obj.env.append_value('CXXFLAGS','-I/Users/yeli/Downloads/ns-allinone-3.35/ns-3.35/examples/rl-fec')
    obj.env.append_value('LINKFLAGS',['-L/Users/yeli/Downloads/ns-allinone-3.35/ns-3.35/examples/rl-fec'])
    obj.env.append_value('LIB', ['streamc'])
