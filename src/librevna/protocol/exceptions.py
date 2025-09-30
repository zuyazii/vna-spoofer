"""
Protocol exceptions for LibreVNA communication

This module defines custom exceptions used in the protocol layer for
handling various error conditions during protocol parsing and generation.

Classes:
- ProtocolError: Base exception for protocol-related errors
- ParseError: Exception for protocol parsing errors
- GenerationError: Exception for protocol generation errors
- ChecksumError: Exception for checksum validation errors
- InvalidCommandError: Exception for invalid command errors

Usage:
    from . import ProtocolError, ParseError
    
    try:
        # Protocol operations
        pass
    except ParseError as e:
        print(f"Parse error: {e}")
    except ProtocolError as e:
        print(f"Protocol error: {e}")
"""


class ProtocolError(Exception):
    """
    Base exception for protocol-related errors
    
    This is the base class for all protocol-related exceptions.
    It provides a common interface for handling protocol errors.
    """
    
    def __init__(self, message: str, error_code: int = None):
        """
        Initialize protocol error
        
        Args:
            message: Error message describing the problem
            error_code: Optional error code for programmatic handling
        """
        super().__init__(message)
        self.message = message
        self.error_code = error_code
    
    def __str__(self) -> str:
        """String representation of the error"""
        if self.error_code is not None:
            return f"ProtocolError {self.error_code}: {self.message}"
        return f"ProtocolError: {self.message}"


class ParseError(ProtocolError):
    """
    Exception for protocol parsing errors
    
    Raised when incoming protocol data cannot be parsed correctly.
    This includes malformed packets, invalid headers, or incomplete data.
    """
    
    def __init__(self, message: str, data: bytes = None, position: int = None):
        """
        Initialize parse error
        
        Args:
            message: Error message describing the parsing problem
            data: Optional raw data that caused the error
            position: Optional position in data where error occurred
        """
        super().__init__(message)
        self.data = data
        self.position = position
    
    def __str__(self) -> str:
        """String representation of the parse error"""
        base_msg = f"ParseError: {self.message}"
        if self.position is not None:
            base_msg += f" at position {self.position}"
        if self.data is not None:
            base_msg += f" (data: {self.data.hex()})"
        return base_msg


class GenerationError(ProtocolError):
    """
    Exception for protocol generation errors
    
    Raised when outgoing protocol data cannot be generated correctly.
    This includes invalid parameters, unsupported commands, or data too large.
    """
    
    def __init__(self, message: str, command: str = None, parameters: dict = None):
        """
        Initialize generation error
        
        Args:
            message: Error message describing the generation problem
            command: Optional command that caused the error
            parameters: Optional parameters that caused the error
        """
        super().__init__(message)
        self.command = command
        self.parameters = parameters
    
    def __str__(self) -> str:
        """String representation of the generation error"""
        base_msg = f"GenerationError: {self.message}"
        if self.command is not None:
            base_msg += f" (command: {self.command})"
        if self.parameters is not None:
            base_msg += f" (parameters: {self.parameters})"
        return base_msg


class ChecksumError(ProtocolError):
    """
    Exception for checksum validation errors
    
    Raised when received data fails checksum validation.
    This indicates data corruption during transmission.
    """
    
    def __init__(self, message: str, expected: int = None, actual: int = None):
        """
        Initialize checksum error
        
        Args:
            message: Error message describing the checksum problem
            expected: Expected checksum value
            actual: Actual checksum value received
        """
        super().__init__(message)
        self.expected = expected
        self.actual = actual
    
    def __str__(self) -> str:
        """String representation of the checksum error"""
        base_msg = f"ChecksumError: {self.message}"
        if self.expected is not None and self.actual is not None:
            base_msg += f" (expected: 0x{self.expected:04x}, actual: 0x{self.actual:04x})"
        return base_msg


class InvalidCommandError(ProtocolError):
    """
    Exception for invalid command errors
    
    Raised when an invalid or unsupported command is encountered.
    This includes unknown command codes or commands with invalid parameters.
    """
    
    def __init__(self, message: str, command_code: int = None):
        """
        Initialize invalid command error
        
        Args:
            message: Error message describing the command problem
            command_code: Optional command code that caused the error
        """
        super().__init__(message)
        self.command_code = command_code
    
    def __str__(self) -> str:
        """String representation of the invalid command error"""
        base_msg = f"InvalidCommandError: {self.message}"
        if self.command_code is not None:
            base_msg += f" (command: 0x{self.command_code:02x})"
        return base_msg
