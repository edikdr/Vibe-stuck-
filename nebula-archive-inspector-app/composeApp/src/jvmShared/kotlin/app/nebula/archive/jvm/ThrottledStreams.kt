package app.nebula.archive

import java.io.InputStream

/**
 * Streams used while serving archive files to a preview.
 *
 * Both live here rather than next to the Android WebView because a desktop preview will serve the same files
 * the same way, and because holding bytes back on a timer is worth a test.
 */

/**
 * Releases bytes no faster than [bytesPerSecond], the way a browser's network throttling does: the reader is
 * held back, so the page really does load slowly instead of merely being reported as slow.
 *
 * Only safe on a worker thread — which is exactly where a preview serves its requests from.
 */
class ThrottledInputStream(
    private val source: InputStream,
    private val bytesPerSecond: Long,
    private val clock: () -> Long = System::currentTimeMillis,
    private val sleep: (Long) -> Unit = { millis -> runCatching { Thread.sleep(millis) } },
) : InputStream() {
    // A separate flag rather than `startedAtMs == 0`: a clock legitimately reads zero at its origin, and
    // treating that as "not started yet" restarts the budget on every read and doubles the delay.
    private var started = false
    private var startedAtMs = 0L
    private var delivered = 0L

    override fun read(): Int {
        val value = source.read()
        if (value >= 0) pace(1)
        return value
    }

    override fun read(buffer: ByteArray, offset: Int, length: Int): Int {
        val count = source.read(buffer, offset, length)
        if (count > 0) pace(count.toLong())
        return count
    }

    private fun pace(bytes: Long) {
        if (bytesPerSecond <= 0) return
        if (!started) {
            started = true
            startedAtMs = clock()
        }
        delivered += bytes
        val expectedMs = delivered * 1_000 / bytesPerSecond
        val elapsedMs = clock() - startedAtMs
        if (expectedMs > elapsedMs) sleep(expectedMs - elapsedMs)
    }

    override fun available(): Int = source.available()

    override fun close() = source.close()
}

/** Stops after [remaining] bytes, so a range request answers exactly the slice that was asked for. */
class LimitedInputStream(
    private val source: InputStream,
    private var remaining: Long,
) : InputStream() {
    override fun read(): Int {
        if (remaining <= 0) return -1
        val value = source.read()
        if (value >= 0) remaining--
        return value
    }

    override fun read(buffer: ByteArray, offset: Int, length: Int): Int {
        if (remaining <= 0) return -1
        val count = source.read(buffer, offset, minOf(length.toLong(), remaining).toInt())
        if (count > 0) remaining -= count
        return count
    }

    override fun close() = source.close()
}
