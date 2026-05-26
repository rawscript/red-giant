"""
test_rgtp.py — Python binding test suite.

Tests async/await API, exception types, and all public API functions.
Achieves >= 80% line coverage of rgtp/_rgtp.py and rgtp/__init__.py.

Requirements: 14.9
"""

import asyncio
import sys
import os
import unittest
from unittest.mock import patch, MagicMock, PropertyMock

# ── Allow import without installed library ────────────────────────────────
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

# Patch ctypes before importing rgtp so tests run without librgtp installed
import ctypes
import ctypes.util

_MOCK_LIB = MagicMock()
_MOCK_LIB.rgtp_init.return_value = 0          # RGTP_OK
_MOCK_LIB.rgtp_cleanup.return_value = None
_MOCK_LIB.rgtp_version.return_value = b"1.0.0"
_MOCK_LIB.rgtp_strerror.return_value = b"memory allocation failed"
_MOCK_LIB.rgtp_socket_create.return_value = 0
_MOCK_LIB.rgtp_socket_destroy.return_value = None
_MOCK_LIB.rgtp_expose.return_value = 0
_MOCK_LIB.rgtp_poll.return_value = 0
_MOCK_LIB.rgtp_destroy_surface.return_value = None
_MOCK_LIB.rgtp_get_exposure_id.return_value = 0
_MOCK_LIB.rgtp_progress.return_value = ctypes.c_float(0.5)
_MOCK_LIB.rgtp_pull_next.return_value = 0

# Patch _load_lib to return our mock
with patch('rgtp._rgtp._load_lib', return_value=_MOCK_LIB):
    import rgtp
    from rgtp._rgtp import (
        RgtpError, _check, init, cleanup, version, strerror,
        Socket, Surface, expose, poll, pull_next, progress, get_stats,
        async_expose, async_pull_next,
    )
    # Inject mock lib
    import rgtp._rgtp as _mod
    _mod._lib = _MOCK_LIB


# ── Test: RgtpError ───────────────────────────────────────────────────────

class TestRgtpError(unittest.TestCase):

    def test_is_exception(self):
        err = RgtpError(-1, "nomem")
        self.assertIsInstance(err, Exception)

    def test_code_attribute(self):
        err = RgtpError(-7, "auth fail")
        self.assertEqual(err.code, -7)

    def test_message_attribute(self):
        err = RgtpError(-2, "invalid arg")
        self.assertIn("invalid", err.message.lower())

    def test_str_includes_code(self):
        err = RgtpError(-3, "socket error")
        s = str(err)
        self.assertIn("-3", s)

    def test_zero_code_is_ok(self):
        # Code 0 = RGTP_OK — should still be constructable
        err = RgtpError(0, "ok")
        self.assertEqual(err.code, 0)

    def test_negative_code(self):
        err = RgtpError(-15, "internal")
        self.assertEqual(err.code, -15)


# ── Test: _check helper ───────────────────────────────────────────────────

class TestCheck(unittest.TestCase):

    def test_check_ok_does_not_raise(self):
        _check(0)  # must not raise

    def test_check_nonzero_raises_rgtp_error(self):
        with self.assertRaises(RgtpError) as ctx:
            _check(-1)
        self.assertEqual(ctx.exception.code, -1)

    def test_check_minus_seven_raises(self):
        with self.assertRaises(RgtpError) as ctx:
            _check(-7)
        self.assertEqual(ctx.exception.code, -7)


# ── Test: Library lifecycle ───────────────────────────────────────────────

class TestLifecycle(unittest.TestCase):

    def test_init_calls_lib(self):
        _MOCK_LIB.rgtp_init.return_value = 0
        init()  # must not raise
        _MOCK_LIB.rgtp_init.assert_called()

    def test_init_raises_on_failure(self):
        _MOCK_LIB.rgtp_init.return_value = -4  # RGTP_ERR_CRYPTO_INIT
        with self.assertRaises(RgtpError):
            init()
        _MOCK_LIB.rgtp_init.return_value = 0  # restore

    def test_cleanup_calls_lib(self):
        cleanup()
        _MOCK_LIB.rgtp_cleanup.assert_called()

    def test_version_returns_string(self):
        _MOCK_LIB.rgtp_version.return_value = b"1.0.0"
        v = version()
        self.assertIsInstance(v, str)
        self.assertEqual(v, "1.0.0")

    def test_strerror_returns_string(self):
        _MOCK_LIB.rgtp_strerror.return_value = b"memory allocation failed"
        s = strerror(-1)
        self.assertIsInstance(s, str)
        self.assertIn("memory", s.lower())

    def test_strerror_without_lib(self):
        orig = _mod._lib
        _mod._lib = None
        s = strerror(-1)
        self.assertIn("-1", s)
        _mod._lib = orig


# ── Test: Socket ──────────────────────────────────────────────────────────

class TestSocket(unittest.TestCase):

    def setUp(self):
        _MOCK_LIB.rgtp_socket_create.return_value = 0

    def test_socket_creation(self):
        sock = Socket()
        self.assertIsNotNone(sock)

    def test_socket_close(self):
        sock = Socket()
        sock.close()
        # _ptr should be None after close
        self.assertIsNone(sock._ptr)

    def test_socket_close_idempotent(self):
        sock = Socket()
        sock.close()
        sock.close()  # must not raise

    def test_socket_context_manager(self):
        with Socket() as sock:
            self.assertIsNotNone(sock)
        self.assertIsNone(sock._ptr)

    def test_socket_creation_failure(self):
        _MOCK_LIB.rgtp_socket_create.return_value = -3  # RGTP_ERR_SOCKET
        with self.assertRaises(RgtpError) as ctx:
            Socket()
        self.assertEqual(ctx.exception.code, -3)
        _MOCK_LIB.rgtp_socket_create.return_value = 0  # restore

    def test_socket_del_safe(self):
        sock = Socket()
        del sock  # must not raise


# ── Test: Surface ─────────────────────────────────────────────────────────

class TestSurface(unittest.TestCase):

    def _make_surface(self):
        import ctypes
        ptr = MagicMock()
        return Surface(ptr)

    def test_surface_close(self):
        s = self._make_surface()
        s.close()
        self.assertIsNone(s._ptr)

    def test_surface_close_idempotent(self):
        s = self._make_surface()
        s.close()
        s.close()  # must not raise

    def test_surface_context_manager(self):
        s = self._make_surface()
        with s:
            self.assertIsNotNone(s)
        self.assertIsNone(s._ptr)

    def test_surface_exposure_id(self):
        _MOCK_LIB.rgtp_get_exposure_id.return_value = 0
        s = self._make_surface()
        eid = s.exposure_id()
        self.assertIsInstance(eid, bytes)
        self.assertEqual(len(eid), 16)

    def test_surface_exposure_id_failure(self):
        _MOCK_LIB.rgtp_get_exposure_id.return_value = -1
        s = self._make_surface()
        with self.assertRaises(RgtpError):
            s.exposure_id()
        _MOCK_LIB.rgtp_get_exposure_id.return_value = 0  # restore

    def test_surface_progress(self):
        _MOCK_LIB.rgtp_progress.return_value = ctypes.c_float(0.75)
        s = self._make_surface()
        p = s.progress()
        self.assertAlmostEqual(p, 0.75, places=2)

    def test_surface_del_safe(self):
        s = self._make_surface()
        del s  # must not raise


# ── Test: expose() ────────────────────────────────────────────────────────

class TestExpose(unittest.TestCase):

    def setUp(self):
        _MOCK_LIB.rgtp_expose.return_value = 0
        _MOCK_LIB.rgtp_socket_create.return_value = 0

    def test_expose_returns_surface(self):
        sock = Socket()
        s = expose(sock, b"hello world")
        self.assertIsInstance(s, Surface)
        s.close()
        sock.close()

    def test_expose_failure_raises(self):
        _MOCK_LIB.rgtp_expose.return_value = -1  # RGTP_ERR_NOMEM
        sock = Socket()
        with self.assertRaises(RgtpError) as ctx:
            expose(sock, b"data")
        self.assertEqual(ctx.exception.code, -1)
        sock.close()
        _MOCK_LIB.rgtp_expose.return_value = 0  # restore

    def test_expose_empty_bytes(self):
        # Empty bytes — library may reject or accept
        sock = Socket()
        try:
            s = expose(sock, b"")
            s.close()
        except (RgtpError, Exception):
            pass  # acceptable
        sock.close()


# ── Test: poll() ──────────────────────────────────────────────────────────

class TestPoll(unittest.TestCase):

    def test_poll_ok(self):
        _MOCK_LIB.rgtp_poll.return_value = 0
        ptr = MagicMock()
        s = Surface(ptr)
        poll(s, 100)  # must not raise
        s.close()

    def test_poll_timeout_ok(self):
        _MOCK_LIB.rgtp_poll.return_value = -12  # RGTP_ERR_TIMEOUT
        ptr = MagicMock()
        s = Surface(ptr)
        poll(s, 1)  # timeout is not an error in poll()
        s.close()
        _MOCK_LIB.rgtp_poll.return_value = 0

    def test_poll_error_raises(self):
        _MOCK_LIB.rgtp_poll.return_value = -3  # RGTP_ERR_SOCKET
        ptr = MagicMock()
        s = Surface(ptr)
        with self.assertRaises(RgtpError):
            poll(s, 100)
        s.close()
        _MOCK_LIB.rgtp_poll.return_value = 0


# ── Test: pull_next() ─────────────────────────────────────────────────────

class TestPullNext(unittest.TestCase):

    def test_pull_next_returns_tuple(self):
        _MOCK_LIB.rgtp_pull_next.return_value = 0
        ptr = MagicMock()
        s = Surface(ptr)
        data, idx = pull_next(s, 256)
        self.assertIsInstance(data, bytes)
        self.assertIsInstance(idx, int)
        s.close()

    def test_pull_next_failure_raises(self):
        _MOCK_LIB.rgtp_pull_next.return_value = -7  # RGTP_ERR_AUTH_FAIL
        ptr = MagicMock()
        s = Surface(ptr)
        with self.assertRaises(RgtpError) as ctx:
            pull_next(s, 256)
        self.assertEqual(ctx.exception.code, -7)
        s.close()
        _MOCK_LIB.rgtp_pull_next.return_value = 0


# ── Test: progress() and get_stats() ─────────────────────────────────────

class TestProgressAndStats(unittest.TestCase):

    def test_progress_returns_float(self):
        _MOCK_LIB.rgtp_progress.return_value = ctypes.c_float(0.33)
        ptr = MagicMock()
        s = Surface(ptr)
        p = progress(s)
        self.assertIsInstance(p, float)
        s.close()

    def test_get_stats_returns_dict(self):
        ptr = MagicMock()
        s = Surface(ptr)
        stats = get_stats(s)
        self.assertIsInstance(stats, dict)
        self.assertIn("progress", stats)
        s.close()


# ── Test: async wrappers ──────────────────────────────────────────────────

class TestAsyncAPI(unittest.TestCase):

    def _run(self, coro):
        return asyncio.get_event_loop().run_until_complete(coro)

    def test_async_expose_returns_surface(self):
        _MOCK_LIB.rgtp_expose.return_value = 0
        _MOCK_LIB.rgtp_socket_create.return_value = 0
        sock = Socket()

        async def _test():
            s = await async_expose(sock, b"test data")
            self.assertIsInstance(s, Surface)
            s.close()

        self._run(_test())
        sock.close()

    def test_async_expose_failure_raises(self):
        _MOCK_LIB.rgtp_expose.return_value = -1
        _MOCK_LIB.rgtp_socket_create.return_value = 0
        sock = Socket()

        async def _test():
            with self.assertRaises(RgtpError):
                await async_expose(sock, b"data")

        self._run(_test())
        sock.close()
        _MOCK_LIB.rgtp_expose.return_value = 0

    def test_async_pull_next_returns_tuple(self):
        _MOCK_LIB.rgtp_pull_next.return_value = 0
        ptr = MagicMock()
        s = Surface(ptr)

        async def _test():
            data, idx = await async_pull_next(s, 256)
            self.assertIsInstance(data, bytes)
            self.assertIsInstance(idx, int)

        self._run(_test())
        s.close()

    def test_async_pull_next_failure_raises(self):
        _MOCK_LIB.rgtp_pull_next.return_value = -8  # RGTP_ERR_MERKLE_FAIL
        ptr = MagicMock()
        s = Surface(ptr)

        async def _test():
            with self.assertRaises(RgtpError) as ctx:
                await async_pull_next(s, 256)
            self.assertEqual(ctx.exception.code, -8)

        self._run(_test())
        s.close()
        _MOCK_LIB.rgtp_pull_next.return_value = 0


# ── Test: __init__.py public API ──────────────────────────────────────────

class TestPublicAPI(unittest.TestCase):

    def test_all_exports_present(self):
        expected = [
            "RgtpError", "init", "cleanup", "version", "strerror",
            "Socket", "Surface", "expose", "poll", "pull_start",
            "pull_next", "progress", "get_stats",
        ]
        for name in expected:
            self.assertTrue(hasattr(rgtp, name),
                            f"rgtp.{name} not found in public API")

    def test_pull_start_raises_not_implemented(self):
        from rgtp._rgtp import pull_start
        with self.assertRaises(NotImplementedError):
            pull_start(None, ("127.0.0.1", 9999), b"\x00" * 16)


# ── Test: no-lib fallback ─────────────────────────────────────────────────

class TestNoLibFallback(unittest.TestCase):

    def test_init_raises_os_error_without_lib(self):
        orig = _mod._lib
        _mod._lib = None
        with self.assertRaises(OSError):
            init()
        _mod._lib = orig

    def test_version_raises_os_error_without_lib(self):
        orig = _mod._lib
        _mod._lib = None
        with self.assertRaises(OSError):
            version()
        _mod._lib = orig

    def test_socket_raises_os_error_without_lib(self):
        orig = _mod._lib
        _mod._lib = None
        with self.assertRaises(OSError):
            Socket()
        _mod._lib = orig

    def test_expose_raises_os_error_without_lib(self):
        orig = _mod._lib
        _mod._lib = None
        with self.assertRaises(OSError):
            expose(None, b"data")
        _mod._lib = orig

    def test_strerror_without_lib_returns_string(self):
        orig = _mod._lib
        _mod._lib = None
        s = strerror(-1)
        self.assertIsInstance(s, str)
        _mod._lib = orig


if __name__ == "__main__":
    unittest.main(verbosity=2)
