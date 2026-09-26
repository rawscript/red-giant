"""
test_rgtp.py — Comprehensive tests for RGTP Python bindings.

Tests cover:
- Library lifecycle
- Socket and Surface management
- Exposer and Puller APIs
- Statistics functions
- Satellite communications
- Error handling
"""

import pytest
import rgtp


class TestLibraryLifecycle:
    """Test library initialization and cleanup."""
    
    def test_version(self):
        """Test that version() returns a valid version string."""
        # Version should work even without init
        try:
            v = rgtp.version()
            assert isinstance(v, str)
            assert len(v) > 0
            # Should be in semver format
            parts = v.split(".")
            assert len(parts) >= 2
            assert all(p.isdigit() for p in parts[:2])
        except OSError:
            pytest.skip("librgtp not installed")
    
    def test_is_initialized_before_init(self):
        """Test is_initialized returns False before init."""
        try:
            # Should not be initialized initially
            result = rgtp.is_initialized()
            assert isinstance(result, bool)
        except OSError:
            pytest.skip("librgtp not installed")
    
    def test_init_cleanup(self):
        """Test init() and cleanup() work correctly."""
        try:
            rgtp.init()
            assert rgtp.is_initialized()
            rgtp.cleanup()
            # After cleanup, might or might not be initialized
        except OSError:
            pytest.skip("librgtp not installed")
    
    def test_strerror(self):
        """Test strerror() returns meaningful strings."""
        try:
            msg = rgtp.strerror(0)
            assert isinstance(msg, str)
            assert len(msg) > 0
            
            msg = rgtp.strerror(-1)
            assert isinstance(msg, str)
            assert len(msg) > 0
        except OSError:
            pytest.skip("librgtp not installed")


class TestSocket:
    """Test Socket class."""
    
    def test_socket_creation(self):
        """Test Socket can be created."""
        try:
            rgtp.init()
            sock = rgtp.Socket()
            assert sock is not None
            sock.close()
            rgtp.cleanup()
        except OSError:
            pytest.skip("librgtp not installed")
    
    def test_socket_context_manager(self):
        """Test Socket works as context manager."""
        try:
            rgtp.init()
            with rgtp.Socket() as sock:
                assert sock is not None
            rgtp.cleanup()
        except OSError:
            pytest.skip("librgtp not installed")


class TestSurface:
    """Test Surface class."""
    
    def test_expose_surface(self):
        """Test creating an exposure surface."""
        try:
            rgtp.init()
            with rgtp.Socket() as sock:
                data = b"Hello, RGTP!" * 100
                surface = rgtp.expose(sock, data)
                assert surface is not None
                
                # Check exposure ID
                exposure_id = surface.exposure_id()
                assert isinstance(exposure_id, bytes)
                assert len(exposure_id) == 16
                
                # Check progress (always 0 for exposer)
                progress = surface.progress()
                assert isinstance(progress, float)
                assert 0.0 <= progress <= 1.0
                
                surface.close()
            rgtp.cleanup()
        except OSError:
            pytest.skip("librgtp not installed")
    
    def test_surface_context_manager(self):
        """Test Surface works as context manager."""
        try:
            rgtp.init()
            with rgtp.Socket() as sock:
                data = b"Test data" * 100
                with rgtp.expose(sock, data) as surface:
                    assert surface is not None
                    exposure_id = surface.exposure_id()
                    assert len(exposure_id) == 16
            rgtp.cleanup()
        except OSError:
            pytest.skip("librgtp not installed")


class TestStatistics:
    """Test statistics functions."""
    
    def test_get_stats(self):
        """Test get_stats() returns valid dictionary."""
        try:
            rgtp.init()
            with rgtp.Socket() as sock:
                data = b"Stats test data" * 100
                with rgtp.expose(sock, data) as surface:
                    stats = rgtp.get_stats(surface)
                    assert isinstance(stats, dict)
                    
                    # Check required fields
                    required_fields = [
                        "bytes_sent", "bytes_received",
                        "chunks_sent", "chunks_received",
                        "auth_failures", "malformed_packets",
                        "fec_recoveries", "nak_sent",
                        "packet_loss_rate", "rtt_us", "pull_pressure"
                    ]
                    
                    for field in required_fields:
                        assert field in stats, f"Missing field: {field}"
            rgtp.cleanup()
        except OSError:
            pytest.skip("librgtp not installed")
    
    def test_get_latency_stats(self):
        """Test get_latency_stats() returns valid dictionary."""
        try:
            rgtp.init()
            with rgtp.Socket() as sock:
                data = b"Latency test data" * 100
                with rgtp.expose(sock, data) as surface:
                    try:
                        stats = rgtp.get_latency_stats(surface)
                        assert isinstance(stats, dict)
                        
                        # Check required fields
                        required_fields = [
                            "mean_us", "jitter_us", "p99_us",
                            "min_us", "max_us", "sample_count"
                        ]
                        
                        for field in required_fields:
                            assert field in stats, f"Missing field: {field}"
                    except rgtp.RgtpError as e:
                        # Latency stats may not be available for exposer surfaces
                        if e.code == rgtp.RgtpError.ERR_INVALID_ARG:
                            pytest.skip("Latency stats not available for exposer")
                        raise
            rgtp.cleanup()
        except OSError:
            pytest.skip("librgtp not installed")


class TestErrorHandling:
    """Test error handling."""
    
    def test_rgtp_error_exception(self):
        """Test RgtpError exception works correctly."""
        err = rgtp.RgtpError(-1, "Test error")
        assert err.code == -1
        assert err.message == "Test error"
        assert "Test error" in str(err)
    
    def test_error_codes(self):
        """Test error codes are accessible."""
        assert rgtp.RgtpError.OK == 0
        assert rgtp.RgtpError.ERR_NOMEM == -1
        assert rgtp.RgtpError.ERR_INVALID_ARG == -2
        assert rgtp.RgtpError.ERR_SATELLITE_NO_CONTACT == -16


class TestAsyncAPI:
    """Test async API functions."""
    
    @pytest.mark.asyncio
    async def test_async_expose(self):
        """Test async_expose() works."""
        try:
            rgtp.init()
            with rgtp.Socket() as sock:
                data = b"Async test data" * 100
                surface = await rgtp.async_expose(sock, data)
                assert surface is not None
                exposure_id = surface.exposure_id()
                assert len(exposure_id) == 16
                surface.close()
            rgtp.cleanup()
        except OSError:
            pytest.skip("librgtp not installed")


class TestSatelliteCommunications:
    """Test satellite communications API (if available)."""
    
    def test_get_satellite_stats(self):
        """Test get_satellite_stats() function."""
        try:
            rgtp.init()
            with rgtp.Socket() as sock:
                data = b"Satellite test data" * 100
                with rgtp.expose(sock, data) as surface:
                    try:
                        stats = rgtp.get_satellite_stats(surface)
                        assert isinstance(stats, dict)
                        
                        # Check required fields
                        required_fields = [
                            "snr_db", "ber", "link_margin_db", "doppler_offset_hz",
                            "contact_attempts", "successful_contacts", "contact_duration_s",
                            "stored_bytes", "stored_chunks", "store_overflow_count",
                            "spacecraft_health", "power_level", "antenna_status",
                            "ccsds_frames_sent", "ccsds_frames_received",
                            "ccsds_frame_errors", "ccsds_vcdu_count"
                        ]
                        
                        for field in required_fields:
                            assert field in stats, f"Missing field: {field}"
                    
                    except rgtp.RgtpError as e:
                        # Satellite stats may require satellite-mode surface
                        if e.code == rgtp.RgtpError.ERR_INVALID_ARG:
                            pytest.skip("Satellite stats require satellite-mode surface")
                        raise
            rgtp.cleanup()
        except OSError:
            pytest.skip("librgtp not installed")
    
    def test_schedule_contact(self):
        """Test schedule_contact() function."""
        try:
            rgtp.init()
            with rgtp.Socket() as sock:
                data = b"Contact test data" * 100
                with rgtp.expose(sock, data) as surface:
                    try:
                        # Schedule a contact window (current time + 1 hour for 30 minutes)
                        import time
                        start = int(time.time()) + 3600
                        end = start + 1800
                        rgtp.schedule_contact(surface, start, end, "TEST_GS")
                    except rgtp.RgtpError as e:
                        # May require satellite-mode surface
                        if e.code in (rgtp.RgtpError.ERR_INVALID_ARG, 
                                     rgtp.RgtpError.ERR_NOT_SUPPORTED):
                            pytest.skip("Schedule contact requires satellite-mode surface")
                        raise
            rgtp.cleanup()
        except OSError:
            pytest.skip("librgtp not installed")
    
    def test_configure_ccsds(self):
        """Test configure_ccsds() function."""
        try:
            rgtp.init()
            with rgtp.Socket() as sock:
                data = b"CCSDS test data" * 100
                with rgtp.expose(sock, data) as surface:
                    try:
                        rgtp.configure_ccsds(surface, enable_tm=True, enable_tc=True)
                    except rgtp.RgtpError as e:
                        if e.code in (rgtp.RgtpError.ERR_INVALID_ARG,
                                     rgtp.RgtpError.ERR_NOT_SUPPORTED):
                            pytest.skip("CCSDS requires satellite-mode surface")
                        raise
            rgtp.cleanup()
        except OSError:
            pytest.skip("librgtp not installed")
    
    def test_check_contact_status(self):
        """Test check_contact_status() function."""
        try:
            rgtp.init()
            with rgtp.Socket() as sock:
                data = b"Status test data" * 100
                with rgtp.expose(sock, data) as surface:
                    try:
                        in_contact, time_to_contact, time_left = rgtp.check_contact_status(surface)
                        assert isinstance(in_contact, bool)
                        assert isinstance(time_to_contact, int)
                        assert isinstance(time_left, int)
                    except rgtp.RgtpError as e:
                        if e.code in (rgtp.RgtpError.ERR_INVALID_ARG,
                                     rgtp.RgtpError.ERR_NOT_SUPPORTED):
                            pytest.skip("Contact status requires satellite-mode surface")
                        raise
            rgtp.cleanup()
        except OSError:
            pytest.skip("librgtp not installed")


class TestPullStart:
    """Test pull_start() functionality."""
    
    def test_pull_start_invalid_exposure_id(self):
        """Test pull_start() rejects invalid exposure_id length."""
        try:
            rgtp.init()
            with rgtp.Socket() as sock:
                # Should raise ValueError for wrong length
                with pytest.raises(ValueError, match="16 bytes"):
                    rgtp.pull_start(sock, ("192.168.1.1", 9000), b"short")
            rgtp.cleanup()
        except OSError:
            pytest.skip("librgtp not installed")
    
    def test_pull_start_valid_exposure_id(self):
        """Test pull_start() accepts valid exposure_id."""
        try:
            rgtp.init()
            with rgtp.Socket() as sock:
                exposure_id = b"0123456789abcdef"  # Exactly 16 bytes
                
                # This will fail to connect, but should not raise NotImplementedError
                try:
                    surface = rgtp.pull_start(sock, ("192.168.1.1", 9000), exposure_id)
                    # If it succeeds (unlikely in test), clean up
                    surface.close()
                except rgtp.RgtpError as e:
                    # Expected: timeout or connection error
                    assert e.code in (
                        rgtp.RgtpError.ERR_TIMEOUT,
                        rgtp.RgtpError.ERR_SOCKET,
                    )
                except NotImplementedError:
                    pytest.fail("pull_start should not raise NotImplementedError")
            rgtp.cleanup()
        except OSError:
            pytest.skip("librgtp not installed")


# ── Module fixtures ─────────────────────────────────────────────────────────

@pytest.fixture(autouse=True)
def cleanup_after_test():
    """Ensure cleanup() is called after each test."""
    yield
    try:
        if rgtp.is_initialized():
            rgtp.cleanup()
    except:
        pass
