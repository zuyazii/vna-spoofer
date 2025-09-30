"""
Error Handling and Recovery

This module implements comprehensive error handling, retry mechanisms,
and graceful degradation for VNA operations.
"""

import time
import logging
import functools
import threading
from typing import Dict, List, Optional, Callable, Any, Union, Type
from dataclasses import dataclass, field
from enum import Enum
import traceback
import signal
import sys

logger = logging.getLogger(__name__)


class ErrorType(Enum):
    """Classification of error types"""
    DEVICE_CONNECTION = "device_connection"
    DEVICE_COMMUNICATION = "device_communication"
    DEVICE_TIMEOUT = "device_timeout"
    DEVICE_ERROR = "device_error"
    CALIBRATION_ERROR = "calibration_error"
    MEASUREMENT_ERROR = "measurement_error"
    DATA_VALIDATION = "data_validation"
    FILE_IO = "file_io"
    CONFIGURATION = "configuration"
    NETWORK = "network"
    UNKNOWN = "unknown"


class ErrorSeverity(Enum):
    """Error severity levels"""
    LOW = "low"          # Minor issues, can continue
    MEDIUM = "medium"    # Significant issues, may need retry
    HIGH = "high"        # Serious issues, operation may fail
    CRITICAL = "critical"  # Fatal issues, operation must stop


@dataclass
class ErrorInfo:
    """Information about an error"""
    error_type: ErrorType
    severity: ErrorSeverity
    message: str
    exception: Optional[Exception] = None
    context: Dict[str, Any] = field(default_factory=dict)
    timestamp: float = field(default_factory=time.time)
    retry_count: int = 0
    max_retries: int = 3
    
    def __post_init__(self):
        """Set timestamp if not provided"""
        if not hasattr(self, 'timestamp') or self.timestamp == 0:
            self.timestamp = time.time()


@dataclass
class RetryConfig:
    """Configuration for retry behavior"""
    max_retries: int = 3
    base_delay: float = 1.0  # seconds
    max_delay: float = 60.0  # seconds
    exponential_backoff: bool = True
    jitter: bool = True
    retry_on_errors: List[ErrorType] = field(default_factory=lambda: [
        ErrorType.DEVICE_TIMEOUT,
        ErrorType.DEVICE_COMMUNICATION,
        ErrorType.NETWORK
    ])
    abort_on_errors: List[ErrorType] = field(default_factory=lambda: [
        ErrorType.CONFIGURATION,
        ErrorType.DATA_VALIDATION
    ])


class TimeoutHandler:
    """Handle timeouts for operations"""
    
    def __init__(self, timeout_seconds: float = 30.0):
        """
        Initialize timeout handler
        
        Args:
            timeout_seconds: Default timeout in seconds
        """
        self.timeout_seconds = timeout_seconds
        self._timeout_occurred = False
    
    def __enter__(self):
        """Enter timeout context"""
        self._timeout_occurred = False
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Exit timeout context"""
        pass
    
    def timeout_handler(self, signum, frame):
        """Handle timeout signal"""
        self._timeout_occurred = True
        raise TimeoutError(f"Operation timed out after {self.timeout_seconds} seconds")
    
    def with_timeout(self, func: Callable, timeout_seconds: Optional[float] = None) -> Any:
        """
        Execute function with timeout
        
        Args:
            func: Function to execute
            timeout_seconds: Timeout in seconds (uses default if None)
            
        Returns:
            Function result
            
        Raises:
            TimeoutError: If operation times out
        """
        timeout = timeout_seconds or self.timeout_seconds
        
        # Set up signal handler for timeout
        old_handler = signal.signal(signal.SIGALRM, self.timeout_handler)
        signal.alarm(int(timeout))
        
        try:
            result = func()
            return result
        finally:
            # Restore original signal handler
            signal.alarm(0)
            signal.signal(signal.SIGALRM, old_handler)


class ErrorRecovery:
    """Error recovery strategies"""
    
    @staticmethod
    def device_reconnect(device_manager, max_attempts: int = 3) -> bool:
        """
        Attempt to reconnect to device
        
        Args:
            device_manager: Device manager instance
            max_attempts: Maximum reconnection attempts
            
        Returns:
            True if reconnection successful
        """
        for attempt in range(max_attempts):
            try:
                logger.info(f"Attempting device reconnection (attempt {attempt + 1}/{max_attempts})")
                
                # Disconnect and reconnect
                if hasattr(device_manager, 'disconnect'):
                    device_manager.disconnect()
                
                time.sleep(1.0)  # Wait before reconnecting
                
                if hasattr(device_manager, 'connect'):
                    device_manager.connect()
                
                logger.info("Device reconnection successful")
                return True
                
            except Exception as e:
                logger.warning(f"Reconnection attempt {attempt + 1} failed: {e}")
                if attempt < max_attempts - 1:
                    time.sleep(2.0 ** attempt)  # Exponential backoff
        
        logger.error("All reconnection attempts failed")
        return False
    
    @staticmethod
    def reset_device_settings(device_manager) -> bool:
        """
        Reset device to default settings
        
        Args:
            device_manager: Device manager instance
            
        Returns:
            True if reset successful
        """
        try:
            logger.info("Resetting device settings")
            
            # Reset to default settings
            if hasattr(device_manager, 'reset_settings'):
                device_manager.reset_settings()
            
            time.sleep(0.5)  # Allow time for reset
            return True
            
        except Exception as e:
            logger.error(f"Device reset failed: {e}")
            return False
    
    @staticmethod
    def clear_measurement_buffer(device_manager) -> bool:
        """
        Clear measurement buffer
        
        Args:
            device_manager: Device manager instance
            
        Returns:
            True if buffer clear successful
        """
        try:
            logger.info("Clearing measurement buffer")
            
            if hasattr(device_manager, 'clear_buffer'):
                device_manager.clear_buffer()
            
            return True
            
        except Exception as e:
            logger.error(f"Buffer clear failed: {e}")
            return False


class ErrorHandler:
    """
    Comprehensive error handling system
    
    This class provides error classification, retry mechanisms,
    and recovery strategies for VNA operations.
    """
    
    def __init__(self, retry_config: Optional[RetryConfig] = None):
        """
        Initialize error handler
        
        Args:
            retry_config: Retry configuration
        """
        self.retry_config = retry_config or RetryConfig()
        self.error_history: List[ErrorInfo] = []
        self.recovery_strategies: Dict[ErrorType, List[Callable]] = {
            ErrorType.DEVICE_CONNECTION: [ErrorRecovery.device_reconnect],
            ErrorType.DEVICE_COMMUNICATION: [
                ErrorRecovery.clear_measurement_buffer,
                ErrorRecovery.reset_device_settings
            ],
            ErrorType.DEVICE_ERROR: [ErrorRecovery.reset_device_settings],
        }
    
    def classify_error(self, exception: Exception, context: Dict[str, Any] = None) -> ErrorInfo:
        """
        Classify error and determine handling strategy
        
        Args:
            exception: Exception that occurred
            context: Additional context information
            
        Returns:
            ErrorInfo with classification
        """
        context = context or {}
        
        # Classify error type
        error_type = self._determine_error_type(exception)
        severity = self._determine_severity(exception, error_type)
        
        error_info = ErrorInfo(
            error_type=error_type,
            severity=severity,
            message=str(exception),
            exception=exception,
            context=context
        )
        
        # Add to history
        self.error_history.append(error_info)
        
        return error_info
    
    def _determine_error_type(self, exception: Exception) -> ErrorType:
        """Determine error type from exception"""
        exception_type = type(exception).__name__
        exception_message = str(exception).lower()
        
        # Device-related errors
        if "timeout" in exception_message or isinstance(exception, TimeoutError):
            return ErrorType.DEVICE_TIMEOUT
        elif "connection" in exception_message or "connect" in exception_message:
            return ErrorType.DEVICE_CONNECTION
        elif "communication" in exception_message or "protocol" in exception_message:
            return ErrorType.DEVICE_COMMUNICATION
        elif "device" in exception_message:
            return ErrorType.DEVICE_ERROR
        
        # Calibration errors
        elif "calibration" in exception_message or "cal" in exception_message:
            return ErrorType.CALIBRATION_ERROR
        
        # Measurement errors
        elif "measurement" in exception_message or "sweep" in exception_message:
            return ErrorType.MEASUREMENT_ERROR
        
        # Data validation errors
        elif "validation" in exception_message or "invalid" in exception_message:
            return ErrorType.DATA_VALIDATION
        
        # File I/O errors
        elif "file" in exception_message or "io" in exception_message:
            return ErrorType.FILE_IO
        
        # Configuration errors
        elif "config" in exception_message or "setting" in exception_message:
            return ErrorType.CONFIGURATION
        
        # Network errors
        elif "network" in exception_message or "socket" in exception_message:
            return ErrorType.NETWORK
        
        else:
            return ErrorType.UNKNOWN
    
    def _determine_severity(self, exception: Exception, error_type: ErrorType) -> ErrorSeverity:
        """Determine error severity"""
        # Critical errors
        if error_type in [ErrorType.CONFIGURATION, ErrorType.DATA_VALIDATION]:
            return ErrorSeverity.CRITICAL
        
        # High severity errors
        elif error_type in [ErrorType.DEVICE_ERROR, ErrorType.CALIBRATION_ERROR]:
            return ErrorSeverity.HIGH
        
        # Medium severity errors
        elif error_type in [ErrorType.DEVICE_CONNECTION, ErrorType.DEVICE_COMMUNICATION]:
            return ErrorSeverity.MEDIUM
        
        # Low severity errors
        elif error_type in [ErrorType.DEVICE_TIMEOUT, ErrorType.NETWORK]:
            return ErrorSeverity.LOW
        
        else:
            return ErrorSeverity.MEDIUM
    
    def should_retry(self, error_info: ErrorInfo) -> bool:
        """
        Determine if operation should be retried
        
        Args:
            error_info: Error information
            
        Returns:
            True if operation should be retried
        """
        # Don't retry if max retries exceeded
        if error_info.retry_count >= error_info.max_retries:
            return False
        
        # Don't retry critical errors
        if error_info.severity == ErrorSeverity.CRITICAL:
            return False
        
        # Don't retry errors in abort list
        if error_info.error_type in self.retry_config.abort_on_errors:
            return False
        
        # Retry errors in retry list
        if error_info.error_type in self.retry_config.retry_on_errors:
            return True
        
        # Default: retry medium and low severity errors
        return error_info.severity in [ErrorSeverity.MEDIUM, ErrorSeverity.LOW]
    
    def get_retry_delay(self, error_info: ErrorInfo) -> float:
        """
        Calculate retry delay
        
        Args:
            error_info: Error information
            
        Returns:
            Delay in seconds before retry
        """
        delay = self.retry_config.base_delay
        
        # Apply exponential backoff
        if self.retry_config.exponential_backoff:
            delay *= (2 ** error_info.retry_count)
        
        # Cap at maximum delay
        delay = min(delay, self.retry_config.max_delay)
        
        # Add jitter to prevent thundering herd
        if self.retry_config.jitter:
            import random
            jitter = random.uniform(0.1, 0.5) * delay
            delay += jitter
        
        return delay
    
    def attempt_recovery(self, error_info: ErrorInfo, context: Dict[str, Any] = None) -> bool:
        """
        Attempt error recovery
        
        Args:
            error_info: Error information
            context: Recovery context (e.g., device_manager)
            
        Returns:
            True if recovery successful
        """
        context = context or {}
        
        if error_info.error_type not in self.recovery_strategies:
            logger.warning(f"No recovery strategy for error type: {error_info.error_type}")
            return False
        
        strategies = self.recovery_strategies[error_info.error_type]
        
        for strategy in strategies:
            try:
                logger.info(f"Attempting recovery strategy: {strategy.__name__}")
                
                # Call recovery strategy with context
                if 'device_manager' in context:
                    success = strategy(context['device_manager'])
                else:
                    success = strategy()
                
                if success:
                    logger.info(f"Recovery successful: {strategy.__name__}")
                    return True
                else:
                    logger.warning(f"Recovery failed: {strategy.__name__}")
                    
            except Exception as e:
                logger.error(f"Recovery strategy {strategy.__name__} failed: {e}")
        
        return False
    
    def handle_error(self, exception: Exception, context: Dict[str, Any] = None) -> ErrorInfo:
        """
        Handle error with classification, recovery, and retry logic
        
        Args:
            exception: Exception that occurred
            context: Additional context
            
        Returns:
            ErrorInfo with handling results
        """
        # Classify error
        error_info = self.classify_error(exception, context)
        
        logger.error(f"Error occurred: {error_info.error_type.value} - {error_info.message}")
        
        # Attempt recovery
        if error_info.severity != ErrorSeverity.CRITICAL:
            recovery_success = self.attempt_recovery(error_info, context)
            if recovery_success:
                logger.info("Error recovery successful")
                return error_info
        
        # Determine if should retry
        if self.should_retry(error_info):
            delay = self.get_retry_delay(error_info)
            logger.info(f"Will retry after {delay:.1f} seconds")
            error_info.retry_count += 1
        else:
            logger.error("No retry possible, operation will fail")
        
        return error_info
    
    def get_error_summary(self) -> Dict[str, Any]:
        """Get summary of error history"""
        if not self.error_history:
            return {'total_errors': 0}
        
        # Count errors by type
        error_counts = {}
        for error in self.error_history:
            error_type = error.error_type.value
            error_counts[error_type] = error_counts.get(error_type, 0) + 1
        
        # Get recent errors
        recent_errors = self.error_history[-5:] if len(self.error_history) > 5 else self.error_history
        
        return {
            'total_errors': len(self.error_history),
            'error_counts': error_counts,
            'recent_errors': [
                {
                    'type': error.error_type.value,
                    'severity': error.severity.value,
                    'message': error.message,
                    'timestamp': error.timestamp,
                    'retry_count': error.retry_count
                }
                for error in recent_errors
            ]
        }


def with_error_handling(error_handler: ErrorHandler = None, 
                       retry_config: RetryConfig = None,
                       context: Dict[str, Any] = None):
    """
    Decorator for automatic error handling
    
    Args:
        error_handler: Error handler instance
        retry_config: Retry configuration
        context: Context for error handling
        
    Returns:
        Decorated function
    """
    def decorator(func: Callable):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            handler = error_handler or ErrorHandler(retry_config)
            context_dict = context or {}
            
            # Add function arguments to context
            context_dict.update({
                'function': func.__name__,
                'args': args,
                'kwargs': kwargs
            })
            
            max_retries = (retry_config.max_retries if retry_config 
                          else handler.retry_config.max_retries)
            
            for attempt in range(max_retries + 1):
                try:
                    return func(*args, **kwargs)
                    
                except Exception as e:
                    error_info = handler.handle_error(e, context_dict)
                    
                    if not handler.should_retry(error_info):
                        raise
                    
                    if attempt < max_retries:
                        delay = handler.get_retry_delay(error_info)
                        logger.info(f"Retrying {func.__name__} after {delay:.1f}s (attempt {attempt + 1}/{max_retries})")
                        time.sleep(delay)
                    else:
                        logger.error(f"All retry attempts exhausted for {func.__name__}")
                        raise
            
        return wrapper
    return decorator


def with_timeout(timeout_seconds: float):
    """
    Decorator for operation timeout
    
    Args:
        timeout_seconds: Timeout in seconds
        
    Returns:
        Decorated function
    """
    def decorator(func: Callable):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            timeout_handler = TimeoutHandler(timeout_seconds)
            return timeout_handler.with_timeout(lambda: func(*args, **kwargs))
        return wrapper
    return decorator
