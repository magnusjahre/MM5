import os, popen2, re, sys

class MyPOpen(object):
    def __init__(self, cmd, input = None, output = None, bufsize = -1):
        self.status = -1

        if input is None:
            p2c_read, p2c_write = os.pipe()
            self.tochild = os.fdopen(p2c_write, 'w', bufsize)
        else:
            p2c_write = None
            if isinstance(input, file):
                p2c_read = input.fileno()
            elif isinstance(input, str):
                input = file(input, 'r')
                p2c_read = input.fileno()
            elif isinstance(input, int):
                p2c_read = input
            else:
                raise AttributeError

        if output is None:
            c2p_read, c2p_write = os.pipe()
            self.fromchild = os.fdopen(c2p_read, 'r', bufsize)
        else:
            c2p_read = None
            if isinstance(output, file):
                c2p_write = output.fileno()
            elif isinstance(output, str):
                output = file(output, 'w')
                c2p_write = output.fileno()
            elif isinstance(output, int):
                c2p_write = output
            else:
                raise AttributeError

        self.pid = os.fork()
        if self.pid == 0:
            os.dup2(p2c_read, sys.stdin.fileno())
            os.dup2(c2p_write, sys.stdout.fileno())
            os.dup2(c2p_write, sys.stderr.fileno())
            try:
                os.execvp(cmd[0], cmd)
            finally:
                os._exit(1)

        os.close(p2c_read)
        os.close(c2p_write)

    def poll(self):
        if self.status < 0:
            pid, status = os.waitpid(self.pid, os.WNOHANG)
            if pid == self.pid:
                self.status = status
        return self.status

    def wait(self):
        if self.status < 0:
            pid, status = os.waitpid(self.pid, 0)
            if pid == self.pid:
                self.status = status
        return self.status

class qsub:
    def __init__(self):
        self.hold = False
        self.join = False
        self.keep_stdout = False
        self.keep_stderr = False
        self.node_type = ''
        self.mail_abort = False
        self.mail_begin = False
        self.mail_end = False
        self.name = ''
        self.stdout = ''
        self.priority = 0
        self.queue = ''
        self.pbshost = ''
        self.qsub = 'qsub'
        self.env = {}

    def build(self, script, args = []):
        self.cmd = [ self.qsub ]

        if self.env:
            arg = '-v'
            arg += ','.join([ '%s=%s' % i for i in self.env.iteritems() ])
            self.cmd.append(arg)

        if self.hold:
            self.cmd.append('-h')

        if len(self.stdout):
            self.cmd.append('-olocalhost:' + self.stdout)

        if self.keep_stdout and self.keep_stderr:
            self.cmd.append('-koe')
        elif self.keep_stdout:
            self.cmd.append('-ko')
        elif self.keep_stderr:
            self.cmd.append('-ke')
        else:
            self.cmd.append('-kn')
            
        if self.join:
            self.cmd.append('-joe')

        if len(self.node_type):
            self.cmd.append('-lnodes=' + self.node_type)

        if self.mail_abort or self.mail_begin or self.mail_end:
            flags = ''
            if self.mail_abort:
                flags.append('a')
            if self.mail_begin:
                flags.append('b')
            if self.mail_end:
                flags.append('e')
            if len(flags):
                self.cmd.append('-m ' + flags)

        if len(self.name):
            self.cmd.append("-N%s" % self.name)

        if self.priority != 0:
            self.cmd.append('-p' + self.priority)

        if len(self.queue):
            self.cmd.append('-q' + self.queue)

        self.cmd.extend(args)
        self.script = script
        self.command = ' '.join(self.cmd + [ self.script ])
        
    def do(self):
        pbs = MyPOpen(self.cmd + [ self.script ])
        self.result = pbs.fromchild.read()
        ec = pbs.wait()

        if ec != 0 and self.pbshost:
            cmd = ' '.join(self.cmd + [ '-' ])
            cmd = [ 'ssh', '-x', self.pbshost, cmd ]
            self.command = ' '.join(cmd)
            ssh = MyPOpen(cmd, input = self.script)
            self.result = ssh.fromchild.read()
            ec = ssh.wait()
        
        return ec
