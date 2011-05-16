import btparser
import btparser.backtrace

btparser.backtrace.Backtrace()

backtrace, location = btparser.backtrace.parse("test.bt")
if not backtrace:
    print location

backtrace.frames
