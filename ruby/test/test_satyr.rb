require 'test/unit'
require 'satyr'

class SatyrTest < Test::Unit::TestCase
  def test_core_duphash
    path = 'test/data/ureport-1'
    json = IO.read(path)

    report = Satyr::Report.new json
    stacktrace = report.stacktrace
    thread = stacktrace.find_crash_thread

    assert_equal 'b9a440a68b6e0f33f6cd989e114d6c4093964e10', thread.duphash

    expected_out = "Thread\n5f6632d75fd027f5b7b410787f3f06c6bf73eee6+0xbb430\n"
    assert_equal expected_out, thread.duphash(frames: 1, flags: :nohash)

    expected_out = "a" + expected_out
    assert_equal expected_out, thread.duphash(frames: 1, flags: :nohash, prefix: "a")
  end

  def test_invalid_report
    assert_raise Satyr::SatyrError do
      report = Satyr::Report.new ""
    end

    begin
      report = Satyr::Report.new "}"
    rescue Satyr::SatyrError => e
      assert_match /Failed to parse JSON:.*}.*/, e.message
    end
  end

  def test_private_constructors
    assert_raise NoMethodError do
      s = Satyr::Stacktrace.new nil
    end

    assert_raise NoMethodError do
      s = Satyr::Thread.new nil
    end
  end
end
