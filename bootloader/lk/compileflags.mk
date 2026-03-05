
ifeq ($(BSP_LK_CLANG_COMPILE), true)
WERROR=true
GLOBAL_COMPILEFLAGS += -mno-unaligned-access -fno-builtin-bcmp
# the following flags are the only ones that may
# be suppressed from being reported as errors
GLOBAL_COMPILEFLAGS += -Wno-writable-strings
GLOBAL_COMPILEFLAGS += -Wno-reserved-user-defined-literal
GLOBAL_COMPILEFLAGS += -Wno-gnu-anonymous-struct
GLOBAL_COMPILEFLAGS += -Wno-shift-count-overflow
GLOBAL_COMPILEFLAGS += -Wno-gnu-zero-variadic-macro-arguments
GLOBAL_COMPILEFLAGS += -Wno-dollar-in-identifier-extension
GLOBAL_COMPILEFLAGS += -Wno-undefined-inline
GLOBAL_COMPILEFLAGS += -Wno-keyword-macro
GLOBAL_COMPILEFLAGS += -Wno-nested-anon-types
GLOBAL_COMPILEFLAGS += -Wno-unknown-warning-option
GLOBAL_COMPILEFLAGS += -Wno-newline-eof
GLOBAL_COMPILEFLAGS += -Wno-pointer-sign
GLOBAL_COMPILEFLAGS += -Wno-switch

# TODO: for clang, chang follow no-errors to errors step by step
GLOBAL_COMPILEFLAGS += -Wno-error=deprecated-non-prototype
GLOBAL_COMPILEFLAGS += -Wno-error=int-conversion
GLOBAL_COMPILEFLAGS += -Wno-error=incompatible-function-pointer-types
GLOBAL_COMPILEFLAGS += -Wno-error=compare-distinct-pointer-types
GLOBAL_COMPILEFLAGS += -Wno-error=c99-extensions
GLOBAL_COMPILEFLAGS += -Wno-error=inline-asm
GLOBAL_COMPILEFLAGS += -Wno-error=incompatible-pointer-types-discards-qualifiers
GLOBAL_COMPILEFLAGS += -Wno-error=asm-operand-widths
GLOBAL_COMPILEFLAGS += -Wno-error=unused-variable
GLOBAL_COMPILEFLAGS += -Wno-error=incompatible-library-redeclaration
GLOBAL_COMPILEFLAGS += -Wno-error=macro-redefined
GLOBAL_COMPILEFLAGS += -Wno-error=format
GLOBAL_COMPILEFLAGS += -Wno-error=incompatible-pointer-types
GLOBAL_COMPILEFLAGS += -Wno-error=sign-compare
GLOBAL_COMPILEFLAGS += -Wno-error=address-of-packed-member
GLOBAL_COMPILEFLAGS += -Wno-error=unused-but-set-variable
GLOBAL_COMPILEFLAGS += -Wno-error=int-to-pointer-cast
GLOBAL_COMPILEFLAGS += -Wno-error=missing-declarations
GLOBAL_COMPILEFLAGS += -Wno-error=pointer-to-int-cast
GLOBAL_COMPILEFLAGS += -Wno-error=visibility
GLOBAL_COMPILEFLAGS += -Wno-error=shift-negative-value
GLOBAL_COMPILEFLAGS += -Wno-error=format-extra-args
GLOBAL_COMPILEFLAGS += -Wno-error=pointer-bool-conversion
GLOBAL_COMPILEFLAGS += -Wno-error=tautological-pointer-compare
GLOBAL_COMPILEFLAGS += -Wno-error=int-to-void-pointer-cast
GLOBAL_COMPILEFLAGS += -Wno-error=enum-conversion
GLOBAL_COMPILEFLAGS += -Wno-error=unknown-attributes
GLOBAL_COMPILEFLAGS += -Wno-error=invalid-noreturn
GLOBAL_COMPILEFLAGS += -Wno-error=void-pointer-to-int-cast
GLOBAL_COMPILEFLAGS += -Wno-error=self-assign
GLOBAL_COMPILEFLAGS += -Wno-error=null-pointer-subtraction
GLOBAL_COMPILEFLAGS += -Wno-error=format-security
GLOBAL_COMPILEFLAGS += -Wno-error=for-loop-analysis
GLOBAL_COMPILEFLAGS += -Wno-error=extra-tokens
GLOBAL_COMPILEFLAGS += -Wno-error=empty-body
GLOBAL_COMPILEFLAGS += -Wno-error=deprecated-declarations

else
WERROR=false
GLOBAL_COMPILEFLAGS += -Werror=comment
# TODO: enable follow errors step by step
#GLOBAL_COMPILEFLAGS += -Werror=unused-variable
#GLOBAL_COMPILEFLAGS += -Werror=unused-value
#GLOBAL_COMPILEFLAGS += -Werror=unused-but-set-variable
GLOBAL_COMPILEFLAGS += -Werror=strict-prototypes
GLOBAL_COMPILEFLAGS += -Werror=strict-aliasing
GLOBAL_COMPILEFLAGS += -Werror=sizeof-pointer-memaccess
#GLOBAL_COMPILEFLAGS += -Werror=sign-compare
#GLOBAL_COMPILEFLAGS += -Werror=pointer-to-int-cast
#GLOBAL_COMPILEFLAGS += -Werror=pointer-sign
#GLOBAL_COMPILEFLAGS += -Werror=parentheses
#GLOBAL_COMPILEFLAGS += -Werror=overflow
GLOBAL_COMPILEFLAGS += -Werror=missing-field-initializers
GLOBAL_COMPILEFLAGS += -Werror=missing-braces
#GLOBAL_COMPILEFLAGS += -Werror=int-to-pointer-cast
#GLOBAL_COMPILEFLAGS += -Werror=format-extra-args
#GLOBAL_COMPILEFLAGS += -Werror=cast-qual
GLOBAL_COMPILEFLAGS += -Werror=maybe-uninitialized
#GLOBAL_COMPILEFLAGS += -Werror=format=
#GLOBAL_COMPILEFLAGS += -Werror=type-limits
GLOBAL_COMPILEFLAGS += -Werror=old-style-declaration
GLOBAL_COMPILEFLAGS += -Wno-format-contains-nul
endif
