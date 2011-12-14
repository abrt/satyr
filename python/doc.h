/* frame */

#define frame_doc "btparser.Frame - class representing a frame in a thread\n" \
                  "Usage:\n" \
                  "btparser.Frame() - creates an empty frame\n" \
                  "btparser.Frame(str) - parses str and fills the frame object"

#define f_get_function_name_doc "Usage: frame.get_function_name()\n" \
                                "Returns: string - function name"

#define f_set_function_name_doc "Usage: frame.set_function_name(newname)\n" \
                                "newname: string - new function name"

#define f_get_function_type_doc "Usage: frame.get_function_type()\n" \
                                "Returns: string - function type"

#define f_set_function_type_doc "Usage: frame.set_function_type(newtype)\n" \
                                "newtype: string - new function type"

#define f_get_number_doc "Usage: frame.get_number()\n" \
                         "Returns: positive integer - frame number"

#define f_set_number_doc "Usage: frame.set_number(N)\n" \
                         "N: positive integer - new frame number"

#define f_get_source_line_doc "Usage: frame.get_source_line()\n" \
                              "Returns: positive integer - source line number"

#define f_set_source_line_doc "Usage: frame.set_source_line(N)\n" \
                              "N: positive integer - new source line number"

#define f_get_source_file_doc "Usage: frame.get_source_file()\n" \
                              "Returns: string - source file name"

#define f_set_source_file_doc "Usage: frame.set_source_file(newname)\n" \
                              "newname: string - new source file name"

#define f_get_signal_handler_called_doc "Usage: frame.get_signal_handler_called()\n" \
                                        "Returns: integer - 0 = False, 1 = True"

#define f_set_signal_handler_called_doc "Usage: frame.set_signal_handler_called(N)\n" \
                                        "N: integer - 0 = False, anything else = True"

#define f_get_address_doc "Usage: frame.get_address()\n" \
                          "Returns: positive long integer - address"

#define f_set_address_doc "Usage: frame.set_address(N)\n" \
                          "N: positive long integer - new address"

#define f_dup_doc "Usage: frame.dup()\n" \
                  "Returns: btparser.Frame - a new clone of frame\n" \
                  "Clones the frame object. All new structures are independent " \
                  "on the original object."

#define f_calls_func_doc "Usage: frame.calls_func(name)\n" \
                         "name: string - function name\n" \
                         "Returns: integer - 0 = False, 1 = True\n" \
                         "Checks whether the frame represents a call to " \
                         "a function with given name."

#define f_calls_func_in_file_doc "Usage: frame.calls_func_in_file(name, filename)\n" \
                                 "name: string - function name\n" \
                                 "filename: string - file name\n" \
                                 "Returns: integer - 0 = False, 1 = True\n" \
                                 "Checks whether the frame represents a call to " \
                                 "a function with given name from a given file."

#define f_cmp_doc "Usage: frame.cmp(frame2, compare_number)\n" \
                  "frame2: btparser.Frame - another frame to compare\n" \
                  "compare_number: boolean - whether to compare also thread numbers\n" \
                  "Returns: integer - distance\n" \
                  "Compares frame to frame2. Returns 0 if frame = frame2, " \
                  "<0 if frame is 'less' than frame2, >0 if frame is 'more' " \
                  "than frame2."

/* thread */

#define thread_doc "btparser.Thread - class representing a thread in a backtrace\n" \
                   "Usage:\n" \
                   "btparser.Thread() - creates an empty thread\n" \
                   "btparser.Thread(str) - parses str and fills the thread object\n" \
                   "btparser.Thread(str, only_funs=True) - parses list of function names"

#define t_get_number_doc "Usage: thread.get_number()\n" \
                         "Returns: positive integer - thread number"

#define t_set_number_doc "Usage: thread.set_number(N)\n" \
                         "N: positive integer - new thread number"

#define t_dup_doc "Usage: thread.dup()\n" \
                  "Returns: btparser.Thread - a new clone of thread\n" \
                  "Clones the thread object. All new structures are independent " \
                  "on the original object."

#define t_cmp_doc "Usage: thread.cmp(thread2)\n" \
                  "thread2: btparser.Thread - another thread to compare" \
                  "Returns: integer - distance" \
                  "Compares thread to thread2. Returns 0 if thread = thread2, " \
                  "<0 if thread is 'less' than thread2, >0 if thread is 'more' " \
                  "than thread2."

#define t_quality_counts_doc "Usage: thread.quality_counts()\n" \
                             "Returns: tuple (ok, all) - ok representing number of " \
                             "'good' frames, all representing total number of frames\n" \
                             "Counts the number of 'good' frames and the number of all " \
                             "frames. 'Good' means the function name is known (not just '?\?')."

#define t_quality_doc "Usage: thread.quality()\n" \
                      "Returns: float - 0..1, thread quality\n" \
                      "Computes the ratio #good / #all. See quality_counts method for more."

#define t_frames_doc (char *)"A list containing btparser.Frame objects representing " \
                     "frames in a thread."

/* backtrace */

#define backtrace_doc "btparser.Backtrace - class representing a backtrace\n" \
                      "Usage:\n" \
                      "btparser.Backtrace() - creates an empty backtrace\n" \
                      "btparser.Backtrace(str) - parses str and fills the backtrace object"

#define b_dup_doc "Usage: backtrace.dup()\n" \
                  "Returns: btparser.Backtrace - a new clone of backtrace\n" \
                  "Clones the backtrace object. All new structures are independent " \
                  "on the original object."

#define b_find_crash_frame_doc "Usage: backtrace.find_crash_frame()\n" \
                               "Returns: btparser.Frame - crash frame\n" \
                               "Finds crash frame in the backtrace. Also sets the " \
                               "backtrace.crashframe field."

#define b_find_crash_thread_doc "Usage: backtrace.find_crash_thread()\n" \
                                "Returns: btparser.Thread - crash thread\n" \
                                "Finds crash thread in the backtrace. Also sets the " \
                                "backtrace.crashthread field."

#define b_limit_frame_depth_doc "Usage: backtrace.limit_frame_depth(N)\n" \
                                "N: positive integer - frame depth\n" \
                                "Crops all threads to only contain first N frames."

#define b_quality_simple_doc "Usage: backtrace.quality_simple()\n" \
                             "Returns: float - 0..1, backtrace quality\n" \
                             "Computes the quality from backtrace itself."

#define b_quality_complex_doc "Usage: backtrace.quality_complex()\n" \
                              "Returns: float - 0..1, backtrace quality\n" \
                              "Computes the quality from backtrace, crash thread and " \
                              "frames around the crash."

#define b_get_duplication_hash_doc "Usage: backtrace.get_duplication_hash()\n" \
                                   "Returns: string - duplication hash\n" \
                                   "Computes the duplication hash used to compare backtraces."

#define b_crashframe_doc (char *)"Readonly. By default the field contains None. After " \
                         "calling the find_crash_frame method, a reference to " \
                         "btparser.Frame object is stored into the field."

#define b_crashthread_doc (char *)"Readonly. By default the field contains None. After " \
                          "calling the find_crash_thread method, a reference to " \
                          "btparser.Thread object is stored into the field."

#define b_threads_doc (char *)"A list containing the btparser.Thread objects " \
                      "representing threads in the backtrace."

