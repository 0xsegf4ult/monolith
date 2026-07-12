import gdb

class CurCPUFunction(gdb.Function):
    def __init__(self):
        super(CurCPUFunction, self).__init__("curcpu")

    def invoke(self):
        parsed = gdb.parse_and_eval("*((cpu_t*)$gs_base)")
        return parsed;

CurCPUFunction()

class CurTaskFunction(gdb.Function):
    def __init__(self):
        super(CurTaskFunction, self).__init__("curtask")

    def invoke(self):
        return gdb.parse_and_eval("*((cpu_t*)$gs_base)->current_task")

CurTaskFunction()

class CurRQFunction(gdb.Function):
    def __init__(self):
        super(CurRQFunction, self).__init__("currq")

    def invoke(self):
        return gdb.parse_and_eval("sched_pcpu_data[((cpu_t*)$gs_base)->id]")

CurRQFunction()
