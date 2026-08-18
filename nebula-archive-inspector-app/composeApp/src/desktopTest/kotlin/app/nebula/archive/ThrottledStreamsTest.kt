package app.nebula.archive

import java.io.ByteArrayInputStream
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class ThrottledStreamsTest {
    /** A virtual clock keeps the test instant while still proving the pacing maths. */
    private class Clock {
        var now = 0L
        val slept = mutableListOf<Long>()

        fun sleep(millis: Long) {
            slept += millis
            now += millis
        }
    }

    @Test
    fun deliversBytesAtTheRequestedRate() {
        val clock = Clock()
        val stream = ThrottledInputStream(
            source = ByteArrayInputStream(ByteArray(50_000)),
            bytesPerSecond = 50_000,
            clock = { clock.now },
            sleep = clock::sleep,
        )
        val buffer = ByteArray(10_000)
        var read = stream.read(buffer, 0, buffer.size)
        var total = 0L
        while (read > 0) {
            total += read
            read = stream.read(buffer, 0, buffer.size)
        }

        assertEquals(50_000L, total)
        // 50 KB at 50 KB/s is one second of simulated transfer.
        assertEquals(1_000L, clock.now)
    }

    @Test
    fun aFasterProfileWaitsLess() {
        fun elapsedFor(rate: Long): Long {
            val clock = Clock()
            val stream = ThrottledInputStream(ByteArrayInputStream(ByteArray(20_000)), rate, { clock.now }, clock::sleep)
            while (stream.read(ByteArray(4_000), 0, 4_000) > 0) Unit
            return clock.now
        }
        assertTrue(elapsedFor(NetworkProfile.Fast4G.bytesPerSecond) < elapsedFor(NetworkProfile.Slow3G.bytesPerSecond))
    }

    @Test
    fun anUnthrottledProfileNeverSleeps() {
        val clock = Clock()
        val stream = ThrottledInputStream(ByteArrayInputStream(ByteArray(8_000)), 0, { clock.now }, clock::sleep)
        while (stream.read(ByteArray(1_000), 0, 1_000) > 0) Unit

        assertTrue(clock.slept.isEmpty())
        assertEquals(0L, clock.now)
    }

    @Test
    fun limitedStreamStopsAtItsSlice() {
        val stream = LimitedInputStream(ByteArrayInputStream(ByteArray(100) { it.toByte() }), 10)
        val buffer = ByteArray(64)
        val read = stream.read(buffer, 0, buffer.size)

        assertEquals(10, read)
        assertEquals(-1, stream.read(buffer, 0, buffer.size))
    }

    @Test
    fun extractorLimitsAreTheDocumentedOnes() {
        assertEquals(350L * 1024 * 1024, JvmArchiveExtractor.MAX_ARCHIVE_BYTES)
        assertEquals(25L * 1024 * 1024, JvmArchiveExtractor.MAX_HTML_BYTES)
        assertEquals(25_000, JvmArchiveExtractor.MAX_ENTRIES)
    }
}
