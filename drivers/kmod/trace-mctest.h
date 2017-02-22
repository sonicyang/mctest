#undef TRACE_SYSTEM
#define TRACE_SYSTEM mctest

#if !defined(_TRACE_MCTEST_H) || defined(TRACE_HEADER_MULTI_READ)                             
#define _TRACE_MCTEST_H

#include <linux/tracepoint.h>

TRACE_EVENT(mctest_exec_time,
	TP_PROTO(int num, s64 ns),
	TP_ARGS(num, ns),
	TP_STRUCT__entry(
		__field(        int,  num				)
		__field(        s64,  ns				)
	),

	TP_fast_assign(
		__entry->num = num;
		__entry->ns = ns;
	),

	TP_printk("test_num=%d time=%lld", __entry->num, __entry->ns)
);

#endif

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .

#define TRACE_INCLUDE_FILE trace-mctest
#include <trace/define_trace.h>
