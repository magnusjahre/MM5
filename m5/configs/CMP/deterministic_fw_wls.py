workloads = {
1:(['perlbmk', 'ammp', 'parser', 'mgrid'], [1000000000, 1000000000, 1000000000, 1000000000]),
2:(['mcf', 'gcc', 'lucas', 'twolf'], [1000000000, 1000000000, 1000000000, 1000000000]),
3:(['facerec', 'mesa', 'eon', 'eon'], [1000000000, 1000000000, 1000000000, 1020000000]),
4:(['ammp', 'vortex1', 'galgel', 'equake'], [1000000000, 1000000000, 1000000000, 1000000000]),
5:(['gcc', 'apsi', 'galgel', 'crafty'], [1000000000, 1000000000, 1000000000, 1000000000]),
6:(['facerec', 'art', 'applu', 'equake'], [1000000000, 1000000000, 1000000000, 1000000000]),
7:(['gcc', 'parser', 'applu', 'gap'], [1000000000, 1000000000, 1000000000, 1000000000]),
8:(['swim', 'twolf', 'mesa', 'gap'], [1000000000, 1000000000, 1000000000, 1000000000]),
9:(['vortex1', 'apsi', 'fma3d', 'sixtrack'], [1000000000, 1000000000, 1000000000, 1000000000]),
10:(['ammp', 'bzip', 'parser', 'equake'], [1000000000, 1000000000, 1000000000, 1000000000]),
11:(['twolf', 'eon', 'applu', 'vpr'], [1000000000, 1000000000, 1000000000, 1000000000]),
12:(['swim', 'galgel', 'mgrid', 'crafty'], [1000000000, 1000000000, 1000000000, 1000000000]),
13:(['twolf', 'galgel', 'fma3d', 'vpr'], [1000000000, 1000000000, 1000000000, 1000000000]),
14:(['bzip', 'bzip', 'equake', 'vpr'], [1000000000, 1020000000, 1000000000, 1000000000]),
15:(['swim', 'galgel', 'crafty', 'vpr'], [1000000000, 1000000000, 1000000000, 1000000000]),
16:(['mcf', 'mesa', 'mesa', 'wupwise'], [1000000000, 1000000000, 1020000000, 1000000000]),
17:(['perlbmk', 'parser', 'applu', 'apsi'], [1000000000, 1000000000, 1000000000, 1000000000]),
18:(['perlbmk', 'gzip', 'mgrid', 'mgrid'], [1000000000, 1000000000, 1000000000, 1020000000]),
19:(['mcf', 'gcc', 'apsi', 'sixtrack'], [1000000000, 1000000000, 1000000000, 1000000000]),
20:(['ammp', 'gcc', 'art', 'mesa'], [1000000000, 1000000000, 1000000000, 1000000000]),
21:(['perlbmk', 'apsi', 'lucas', 'equake'], [1000000000, 1000000000, 1000000000, 1000000000]),
22:(['mcf', 'crafty', 'vpr', 'vpr'], [1000000000, 1000000000, 1000000000, 1020000000]),
23:(['gzip', 'mesa', 'mgrid', 'equake'], [1000000000, 1000000000, 1000000000, 1000000000]),
24:(['facerec', 'fma3d', 'applu', 'lucas'], [1000000000, 1000000000, 1000000000, 1000000000]),
25:(['facerec', 'parser', 'applu', 'gap'], [1000000000, 1000000000, 1000000000, 1000000000]),
26:(['mcf', 'ammp', 'apsi', 'twolf'], [1000000000, 1000000000, 1000000000, 1000000000]),
27:(['swim', 'ammp', 'sixtrack', 'applu'], [1000000000, 1000000000, 1000000000, 1000000000]),
28:(['swim', 'fma3d', 'parser', 'art'], [1000000000, 1000000000, 1000000000, 1000000000]),
29:(['twolf', 'gcc', 'apsi', 'vortex1'], [1000000000, 1000000000, 1000000000, 1000000000]),
30:(['gzip', 'apsi', 'mgrid', 'equake'], [1000000000, 1000000000, 1000000000, 1000000000]),
31:(['mgrid', 'eon', 'equake', 'vpr'], [1000000000, 1000000000, 1000000000, 1000000000]),
32:(['facerec', 'twolf', 'gap', 'wupwise'], [1000000000, 1000000000, 1000000000, 1000000000]),
33:(['gzip', 'galgel', 'lucas', 'equake'], [1000000000, 1000000000, 1000000000, 1000000000]),
34:(['facerec', 'facerec', 'gcc', 'apsi'], [1000000000, 1020000000, 1000000000, 1000000000]),
35:(['swim', 'mcf', 'mesa', 'sixtrack'], [1000000000, 1000000000, 1000000000, 1000000000]),
36:(['mesa', 'bzip', 'sixtrack', 'equake'], [1000000000, 1000000000, 1000000000, 1000000000]),
37:(['mcf', 'gcc', 'vortex1', 'gap'], [1000000000, 1000000000, 1000000000, 1000000000]),
38:(['facerec', 'mcf', 'parser', 'lucas'], [1000000000, 1000000000, 1000000000, 1000000000]),
39:(['twolf', 'mesa', 'eon', 'eon'], [1000000000, 1000000000, 1000000000, 1020000000]),
40:(['mcf', 'apsi', 'apsi', 'equake'], [1000000000, 1000000000, 1020000000, 1000000000]),

# Amplified miss frequency workloads, first try
#41:(['facerec', 'vortex1', 'galgel', 'sixtrack'], [1000000000, 1000000000, 1000000000, 1000000000]),
#42:(['vortex1', 'galgel', 'mesa', 'applu'], [1000000000, 1000000000, 1000000000, 1000000000]),
#43:(['facerec', 'mesa', 'art', 'applu'], [1000000000, 1000000000, 1000000000, 1000000000]),
#44:(['facerec', 'galgel', 'mgrid', 'applu'], [1000000000, 1000000000, 1000000000, 1000000000]),
#45:(['facerec', 'gzip', 'galgel', 'sixtrack'], [1000000000, 1000000000, 1000000000, 1000000000]),
#46:(['gzip', 'galgel', 'mgrid', 'mesa'], [1000000000, 1000000000, 1000000000, 1000000000]),
#47:(['swim', 'facerec', 'art', 'sixtrack'], [1000000000, 1000000000, 1000000000, 1000000000]),
#48:(['swim', 'facerec', 'art', 'gzip'], [1000000000, 1000000000, 1000000000, 1000000000]),
#49:(['swim', 'facerec', 'mgrid', 'gzip'], [1000000000, 1000000000, 1000000000, 1000000000]),
#50:(['facerec', 'gzip', 'mesa', 'wupwise'], [1000000000, 1000000000, 1000000000, 1000000000])

# Amplified miss frequency workloads
41:(['mcf', 'apsi', 'applu', 'wupwise'], [1000000000, 1000000000, 1000000000, 1000000000]),
42:(['gzip', 'mcf', 'art', 'gap'], [1000000000, 1000000000, 1000000000, 1000000000]),
43:(['gzip', 'mesa', 'galgel', 'applu'], [1000000000, 1000000000, 1000000000, 1000000000]),
44:(['gzip', 'galgel', 'mesa', 'sixtrack'], [1000000000, 1000000000, 1000000000, 1000000000]),
45:(['facerec', 'galgel', 'mgrid', 'vortex1'], [1000000000, 1000000000, 1000000000, 1000000000]),
46:(['gzip', 'mcf', 'mesa', 'art'], [1000000000, 1000000000, 1000000000, 1000000000]),
47:(['swim', 'apsi', 'sixtrack', 'applu'], [1000000000, 1000000000, 1000000000, 1000000000]),
48:(['facerec', 'swim', 'art', 'sixtrack'], [1000000000, 1000000000, 1000000000, 1000000000]),
49:(['galgel', 'apsi', 'art', 'gcc'], [1000000000, 1000000000, 1000000000, 1000000000]),
50:(['mcf', 'mesa', 'vortex1', 'wupwise'], [1000000000, 1000000000, 1000000000, 1000000000]),
51:(['facerec', 'mcf', 'gcc', 'sixtrack'], [1000000000, 1000000000, 1000000000, 1000000000]),
52:(['gzip', 'mcf', 'mesa', 'applu'], [1000000000, 1000000000, 1000000000, 1000000000]),
53:(['galgel', 'apsi', 'applu', 'sixtrack'], [1000000000, 1000000000, 1000000000, 1000000000]),
54:(['swim', 'vortex1', 'apsi', 'art'], [1000000000, 1000000000, 1000000000, 1000000000]),
55:(['swim', 'gzip', 'mesa', 'applu'], [1000000000, 1000000000, 1000000000, 1000000000]),
56:(['vortex1', 'galgel', 'mesa', 'sixtrack'], [1000000000, 1000000000, 1000000000, 1000000000]),
57:(['wupwise', 'vortex1', 'apsi', 'gap'], [1000000000, 1000000000, 1000000000, 1000000000]),
58:(['mcf', 'mesa', 'vortex1', 'gcc'], [1000000000, 1000000000, 1000000000, 1000000000]),
59:(['mcf', 'galgel', 'vortex1', 'applu'], [1000000000, 1000000000, 1000000000, 1000000000]),
60:(['mesa', 'applu', 'sixtrack', 'gap'], [1000000000, 1000000000, 1000000000, 1000000000]),
61:(['swim', 'mesa', 'art', 'sixtrack'], [1000000000, 1000000000, 1000000000, 1000000000]),
62:(['swim', 'mcf', 'gcc', 'wupwise'], [1000000000, 1000000000, 1000000000, 1000000000]),
63:(['mesa', 'apsi', 'vortex1', 'sixtrack'], [1000000000, 1000000000, 1000000000, 1000000000]),
64:(['art', 'galgel', 'mgrid', 'gap'], [1000000000, 1000000000, 1000000000, 1000000000]),
65:(['gzip', 'mesa', 'apsi', 'gcc'], [1000000000, 1000000000, 1000000000, 1000000000]),
66:(['galgel', 'apsi', 'art', 'gcc'], [1000000000, 1000000000, 1000000000, 1000000000]),
67:(['facerec', 'vortex1', 'art', 'gap'], [1000000000, 1000000000, 1000000000, 1000000000]),
68:(['vortex1', 'mcf', 'mesa', 'applu'], [1000000000, 1000000000, 1000000000, 1000000000]),
69:(['swim', 'gcc', 'vortex1', 'gap'], [1000000000, 1000000000, 1000000000, 1000000000]),
70:(['swim', 'gzip', 'galgel', 'art'], [1000000000, 1000000000, 1000000000, 1000000000]),
71:(['swim', 'gzip', 'galgel', 'wupwise'], [1000000000, 1000000000, 1000000000, 1000000000]),
72:(['gzip', 'mcf', 'mesa', 'wupwise'], [1000000000, 1000000000, 1000000000, 1000000000]),
73:(['wupwise', 'apsi', 'art', 'gap'], [1000000000, 1000000000, 1000000000, 1000000000]),
74:(['art', 'apsi', 'mgrid', 'gap'], [1000000000, 1000000000, 1000000000, 1000000000]),
75:(['swim', 'mesa', 'mgrid', 'wupwise'], [1000000000, 1000000000, 1000000000, 1000000000]),
76:(['facerec', 'mcf', 'art', 'sixtrack'], [1000000000, 1000000000, 1000000000, 1000000000]),
77:(['facerec', 'gzip', 'gcc', 'gap'], [1000000000, 1000000000, 1000000000, 1000000000]),
78:(['facerec', 'mcf', 'gcc', 'sixtrack'], [1000000000, 1000000000, 1000000000, 1000000000]),
79:(['facerec', 'swim', 'vortex1', 'gzip'], [1000000000, 1000000000, 1000000000, 1000000000]),
80:(['facerec', 'mcf', 'mgrid', 'sixtrack'], [1000000000, 1000000000, 1000000000, 1000000000])

}


def getBms(wl):
    num = int(wl.replace("fair",""))
    return workloads[num][0]
