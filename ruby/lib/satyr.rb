require 'ffi'

# Ruby bindings for libsatyr
module Satyr

  # FFI wrappers for C functions. <em>Do not use this module directly</em>
  module FFI
    extend ::FFI::Library

    ffi_lib 'libsatyr.so.3', ::FFI::Library::LIBC

    enum :report_type, [ :invalid, 0,
                         :core,
                         :python,
                         :kerneloops,
                         :java,
                         :gdb ]

    enum :duphash_flags, [ :normal, 1 << 0,
                           :nohash, 1 << 1,
                           :nonormalize, 1 << 2,
                           :koops_compat, 1 << 3 ]

    attach_function :sr_report_from_json_text, [:string, :pointer], :pointer
    attach_function :sr_report_free, [:pointer], :void

    attach_function :sr_stacktrace_find_crash_thread, [:pointer], :pointer
    attach_function :sr_core_stacktrace_dup, [:pointer], :pointer
    attach_function :sr_python_stacktrace_dup, [:pointer], :pointer
    attach_function :sr_koops_stacktrace_dup, [:pointer], :pointer
    attach_function :sr_java_stacktrace_dup, [:pointer], :pointer
    attach_function :sr_gdb_stacktrace_dup, [:pointer], :pointer
    attach_function :sr_stacktrace_free, [:pointer], :void

    attach_function :sr_thread_dup, [:pointer], :pointer
    attach_function :sr_thread_get_duphash, [:pointer, :int, :string, :duphash_flags], :pointer
    attach_function :sr_thread_free, [:pointer], :void

    attach_function :free, [:pointer], :void

    class ReportStruct < ::FFI::ManagedStruct
      layout :version, :uint32,
        :type, :report_type,
        :reporter_name, :string,
        :reporter_version, :string,
        :user_root, :bool,
        :user_local, :bool,
        :operating_system, :pointer,
        :component_name, :string,
        :rpm_packages, :pointer,
        :stacktrace, :pointer

      def self.release(pointer)
        Satyr::FFI.sr_report_free pointer
      end
    end

    class StacktraceStruct < ::FFI::ManagedStruct
      layout :type, :report_type

      def self.release(pointer)
        Satyr::FFI.sr_stacktrace_free pointer
      end
    end

    class ThreadStruct < ::FFI::ManagedStruct
      layout :type, :report_type

      def self.release(pointer)
        Satyr::FFI.sr_thread_free pointer
      end
    end

  end

  # Exception for errors originating in satyr
  class SatyrError < StandardError
  end

  # Report containing all data relevant to a software problem
  class Report

    # Parses given JSON string to create new Report
    def initialize(json)
      error_msg = ::FFI::MemoryPointer.new(:pointer, 1)
      pointer = Satyr::FFI.sr_report_from_json_text json.to_s, error_msg
      error_msg = error_msg.read_pointer
      unless error_msg.null?
        message = error_msg.read_string.force_encoding('UTF-8')
        Satyr::FFI.free error_msg
        raise SatyrError, "Failed to parse JSON: #{message}"
      end

      # from_json_text should never return NULL without setting error_msg,
      # better err on the safe side though
      raise SatyrError, "Failed to create stacktrace" if pointer.null?

      @struct = Satyr::FFI::ReportStruct.new pointer
    end

    # Returns Stacktrace of the report
    def stacktrace
      stacktrace = @struct[:stacktrace]
      return nil if stacktrace.null?

      # There's no sr_stacktrace_dup in libsatyr.so.3, we've got to find out
      # the type ourselves.
      dup = case @struct[:type]
      when :core
        Satyr::FFI.sr_core_stacktrace_dup(stacktrace)
      when :python
        Satyr::FFI.sr_python_stacktrace_dup(stacktrace)
      when :kerneloops
        Satyr::FFI.sr_koops_stacktrace_dup(stacktrace)
      when :java
        Satyr::FFI.sr_java_stacktrace_dup(stacktrace)
      when :gdb
        Satyr::FFI.sr_gdb_stacktrace_dup(stacktrace)
      else
        raise SatyrError, "Invalid report type"
      end

      raise SatyrError, "Failed to obtain stacktrace" if dup.null?

      Stacktrace.send(:new, dup)
    end
  end

  # Program stack trace, possibly composed of multiple threads
  class Stacktrace

    # Private constructor - Stacktrace objects are created in Report#stacktrace
    def initialize(pointer)
      raise SatyrError, "Invalid structure" if pointer.null?
      @struct = Satyr::FFI::StacktraceStruct.new pointer
    end
    private_class_method :new

    # Returns Thread that likely caused the crash that produced this stack
    # trace
    def find_crash_thread
      pointer = Satyr::FFI.sr_stacktrace_find_crash_thread @struct.to_ptr
      raise SatyrError, "Could not find crash thread" if pointer.null?
      dup = Satyr::FFI.sr_thread_dup(pointer)
      raise SatyrError, "Failed to duplicate thread" if dup.null?

      Thread.send(:new, dup)
    end
  end

  # Thread in a stack trace containing individual stack frames
  class Thread

    # Private constructor - Thread objects are returned by Stacktrace methods
    def initialize(pointer)
      raise SatyrError, "Invalid structure" if pointer.null?
      @struct = Satyr::FFI::ThreadStruct.new pointer
    end
    private_class_method :new

    # Duplication hash for the thread
    # The method takes an option hash with following keys:
    # - +:frames+:: number of frames to use (default 0 means use all)
    # - +:flags+:: bitwise sum of (:normal, :nohash, :nonormalize,
    #              :koops_compat)
    # - +:prefix+:: string to be prepended in front of the text before hashing
    def duphash(opts = {})
      opts = {:frames => 0, :flags => :normal, :prefix => ""}.merge(opts)
      str_pointer = Satyr::FFI.sr_thread_get_duphash @struct.to_ptr, opts[:frames], opts[:prefix], opts[:flags]
      raise SatyrError, "Failed to compute duphash" if str_pointer.null?
      hash = str_pointer.read_string.force_encoding('UTF-8')
      Satyr::FFI.free str_pointer
      hash
    end
  end
end
