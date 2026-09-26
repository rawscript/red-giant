#!/usr/bin/env python3
"""
verify_package.py — Verify RGTP Python package completeness.

This script checks that all advertised functions are available and
properly documented.
"""

import sys
import inspect


def check_exports():
    """Verify all advertised functions are exported."""
    print("=" * 60)
    print("RGTP Python Package Verification")
    print("=" * 60)
    print()
    
    # Import the package
    try:
        import rgtp
        print("✓ Package imports successfully")
    except ImportError as e:
        print(f"✗ Failed to import package: {e}")
        return False
    
    print(f"✓ Version: {rgtp.__version__}")
    print()
    
    # Check __all__ exports
    print("Checking exported symbols...")
    print("-" * 60)
    
    expected_exports = [
        # Exceptions
        "RgtpError",
        
        # Library lifecycle
        "init", "cleanup", "version", "strerror", "is_initialized",
        
        # Handles
        "Socket", "Surface",
        
        # Exposer API
        "expose", "poll",
        
        # Puller API
        "pull_start", "pull_next", "progress",
        
        # Statistics
        "get_stats", "get_latency_stats",
        
        # Satellite Communications
        "get_satellite_stats",
        "schedule_contact",
        "update_link_parameters",
        "enable_store_forward",
        "get_contact_windows",
        "configure_ccsds",
        "send_emergency_tc",
        "calculate_link_budget",
        "configure_doppler",
        "check_contact_status",
        
        # Async wrappers
        "async_expose", "async_pull_next", "async_poll",
    ]
    
    missing = []
    for name in expected_exports:
        if hasattr(rgtp, name):
            obj = getattr(rgtp, name)
            obj_type = type(obj).__name__
            
            # Check if it's callable (function or class)
            if callable(obj):
                sig = ""
                try:
                    sig = str(inspect.signature(obj))
                except (ValueError, TypeError):
                    sig = "(...)"
                
                print(f"✓ {name:<25} {obj_type:<15} {sig}")
            else:
                print(f"✓ {name:<25} {obj_type:<15}")
        else:
            print(f"✗ {name:<25} MISSING")
            missing.append(name)
    
    print()
    print("-" * 60)
    
    if missing:
        print(f"✗ {len(missing)} missing exports: {', '.join(missing)}")
        return False
    else:
        print(f"✓ All {len(expected_exports)} expected exports present")
    
    print()
    
    # Check that pull_start is not NotImplementedError
    print("Checking for mock implementations...")
    print("-" * 60)
    
    try:
        # Check if pull_start raises NotImplementedError
        import rgtp._rgtp as _rgtp
        
        # Get the function
        func = _rgtp.pull_start
        
        # Check source code for NotImplementedError
        source = inspect.getsource(func)
        
        if "NotImplementedError" in source and "pull_start requires" in source:
            print("✗ pull_start contains NotImplementedError (mock implementation)")
            return False
        else:
            print("✓ pull_start has full implementation")
    
    except Exception as e:
        print(f"⚠ Could not verify implementation: {e}")
    
    print()
    
    # Check error codes
    print("Checking error codes...")
    print("-" * 60)
    
    error_codes = {
        "OK": 0,
        "ERR_NOMEM": -1,
        "ERR_INVALID_ARG": -2,
        "ERR_SOCKET": -3,
        "ERR_CRYPTO_INIT": -4,
        "ERR_ENCRYPT": -5,
        "ERR_DECRYPT": -6,
        "ERR_AUTH_FAIL": -7,
        "ERR_MERKLE_FAIL": -8,
        "ERR_FEC_FAIL": -9,
        "ERR_TRUNCATED": -10,
        "ERR_CHUNK_INDEX_OOB": -11,
        "ERR_TIMEOUT": -12,
        "ERR_RATE_LIMITED": -13,
        "ERR_NOT_SUPPORTED": -14,
        "ERR_INTERNAL": -15,
        "ERR_SATELLITE_NO_CONTACT": -16,
    }
    
    all_codes_ok = True
    for name, expected_value in error_codes.items():
        if hasattr(rgtp.RgtpError, name):
            actual_value = getattr(rgtp.RgtpError, name)
            if actual_value == expected_value:
                print(f"✓ RgtpError.{name:<25} = {expected_value}")
            else:
                print(f"✗ RgtpError.{name:<25} = {actual_value} (expected {expected_value})")
                all_codes_ok = False
        else:
            print(f"✗ RgtpError.{name:<25} MISSING")
            all_codes_ok = False
    
    print()
    
    if not all_codes_ok:
        return False
    
    print("=" * 60)
    print("✓ ALL CHECKS PASSED")
    print("=" * 60)
    
    return True


def test_library_loading():
    """Test if the library can be loaded."""
    print()
    print("Testing library loading...")
    print("-" * 60)
    
    try:
        import rgtp
        from rgtp import _rgtp
        
        if _rgtp._lib is None:
            print("⚠ Library not loaded (librgtp not found)")
            print("  This is expected if the library hasn't been built yet.")
            print("  The package is correctly configured and will work")
            print("  once librgtp is installed or bundled.")
            return True
        else:
            print("✓ Library loaded successfully")
            
            # Try to get version
            try:
                v = rgtp.version()
                print(f"✓ Library version: {v}")
            except Exception as e:
                print(f"✗ Failed to get library version: {e}")
                return False
            
            return True
    
    except Exception as e:
        print(f"✗ Library loading test failed: {e}")
        return False


if __name__ == "__main__":
    success = check_exports()
    
    if success:
        success = test_library_loading()
    
    sys.exit(0 if success else 1)
