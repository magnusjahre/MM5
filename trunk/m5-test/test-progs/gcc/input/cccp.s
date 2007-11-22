	file	 "cccp.i"
data

; cc1 (2.7.2.2) arguments: -O -fdefer-pop -fomit-frame-pointer
; -fcse-follow-jumps -fcse-skip-blocks -fexpensive-optimizations
; -fthread-jumps -fstrength-reduce -funroll-loops -fwritable-strings
; -fpeephole -fforce-mem -ffunction-cse -finline-functions -finline
; -freg-struct-return -fdelayed-branch -frerun-cse-after-loop
; -fschedule-insns -fschedule-insns2 -fcommon -fgnu-linker -m88110 -m88100
; -m88000 -mno-ocs-debug-info -mno-ocs-frame-position -mcheck-zero-division

gcc2_compiled.:
	align	 4
_lint:
	word	 0
	align	 4
_put_out_comments:
	word	 0
	align	 4
_no_trigraphs:
	word	 0
	align	 4
_print_deps:
	word	 0
	align	 4
_print_include_names:
	word	 0
	align	 4
_dump_macros:
	word	 0
	align	 4
_debug_output:
	word	 0
	align	 4
_inhibit_warnings:
	word	 0
	align	 4
_warn_import:
	word	 1
	align	 4
_done_initializing:
	word	 0
	align	 4
_indepth:
	word	 -1
	align	 4
_system_include_depth:
	word	 0
	align	 4
_include_defaults_array:
	word	 @LC0
	word	 1
	word	 @LC1
	word	 0
	word	 @LC2
	word	 0
	word	 @LC3
	word	 0
	word	 0
	word	 0
	align	 8
@LC3:
	string	 "/usr/include\000"
	align	 8
@LC2:
	string	 "/usr/local/include\000"
	align	 8
@LC1:
	string	 "/usr/local/bin\000"
	align	 8
@LC0:
	string	 "/usr\000"
	align	 4
_include_defaults:
	word	 _include_defaults_array
	align	 4
_include:
	word	 0
	align	 4
_first_bracket_include:
	word	 0
	align	 4
_first_system_include:
	word	 0
	align	 4
_last_include:
	word	 0
	align	 4
_after_include:
	word	 0
	align	 4
_last_after_include:
	word	 0
	align	 4
_dont_repeat_files:
	word	 0
	align	 4
_all_include_files:
	word	 0
	align	 4
_stringlist_tailp:
	word	 _stringlist
	align	 8
_rest_extension:
	string	 "...\000"
	align	 8
@LC4:
	string	 "-Dunix -D__osf__ -D__alpha -D__alpha__ -D_LONGLON"
	string	 "G -DSYSTYPE_BSD  -D_SYSTYPE_BSD\000"
	align	 4
_predefs:
	word	 @LC4
	align	 4
_directive_table:
	word	 6
	word	 _do_define
	word	 @LC5
	word	 1
	byte	 0
	byte	 1
	zero	 2
	word	 2
	word	 _do_if
	word	 @LC6
	word	 7
	zero	 4
	word	 5
	word	 _do_xifdef
	word	 @LC7
	word	 5
	zero	 4
	word	 6
	word	 _do_xifdef
	word	 @LC8
	word	 6
	zero	 4
	word	 5
	word	 _do_endif
	word	 @LC9
	word	 15
	zero	 4
	word	 4
	word	 _do_else
	word	 @LC10
	word	 8
	zero	 4
	word	 4
	word	 _do_elif
	word	 @LC11
	word	 10
	zero	 4
	word	 4
	word	 _do_line
	word	 @LC12
	word	 12
	zero	 4
	word	 7
	word	 _do_include
	word	 @LC13
	word	 2
	byte	 1
	zero	 3
	word	 12
	word	 _do_include
	word	 @LC14
	word	 3
	byte	 1
	zero	 3
	word	 6
	word	 _do_include
	word	 @LC15
	word	 4
	byte	 1
	zero	 3
	word	 5
	word	 _do_undef
	word	 @LC16
	word	 11
	zero	 4
	word	 5
	word	 _do_error
	word	 @LC17
	word	 13
	zero	 4
	word	 7
	word	 _do_warning
	word	 @LC18
	word	 14
	zero	 4
	word	 6
	word	 _do_pragma
	word	 @LC19
	word	 9
	byte	 0
	byte	 0
	byte	 1
	zero	 1
	word	 5
	word	 _do_ident
	word	 @LC20
	word	 17
	byte	 0
	byte	 0
	byte	 1
	zero	 1
	word	 6
	word	 _do_assert
	word	 @LC21
	word	 18
	zero	 4
	word	 8
	word	 _do_unassert
	word	 @LC22
	word	 19
	zero	 4
	word	 -1
	word	 0
	word	 @LC23
	word	 35
	zero	 4
	align	 8
@LC23:
	string	 "\000"
	align	 8
@LC22:
	string	 "unassert\000"
	align	 8
@LC21:
	string	 "assert\000"
	align	 8
@LC20:
	string	 "ident\000"
	align	 8
@LC19:
	string	 "pragma\000"
	align	 8
@LC18:
	string	 "warning\000"
	align	 8
@LC17:
	string	 "error\000"
	align	 8
@LC16:
	string	 "undef\000"
	align	 8
@LC15:
	string	 "import\000"
	align	 8
@LC14:
	string	 "include_next\000"
	align	 8
@LC13:
	string	 "include\000"
	align	 8
@LC12:
	string	 "line\000"
	align	 8
@LC11:
	string	 "elif\000"
	align	 8
@LC10:
	string	 "else\000"
	align	 8
@LC9:
	string	 "endif\000"
	align	 8
@LC8:
	string	 "ifndef\000"
	align	 8
@LC7:
	string	 "ifdef\000"
	align	 8
@LC6:
	string	 "if\000"
	align	 8
@LC5:
	string	 "define\000"
	align	 4
_errors:
	word	 0
	align	 4
_if_stack:
	word	 0
	align	 8
@LC24:
	string	 "Usage: %s [switches] input output\000"
	align	 8
@LC25:
	string	 "-include\000"
	align	 8
@LC26:
	string	 "Filename missing after -include option\000"
	align	 8
@LC27:
	string	 "-imacros\000"
	align	 8
@LC28:
	string	 "Filename missing after -imacros option\000"
	align	 8
@LC29:
	string	 "-iprefix\000"
	align	 8
@LC30:
	string	 "Filename missing after -iprefix option\000"
	align	 8
@LC31:
	string	 "-idirafter\000"
	align	 8
@LC32:
	string	 "Directory name missing after -idirafter option\000"
	align	 8
@LC33:
	string	 "Output filename specified twice\000"
	align	 8
@LC34:
	string	 "Filename missing after -o option\000"
	align	 8
@LC35:
	string	 "-\000"
	align	 8
@LC36:
	string	 "\000"
	align	 8
@LC37:
	string	 "-pedantic\000"
	align	 8
@LC38:
	string	 "-pedantic-errors\000"
	align	 8
@LC39:
	string	 "-pcp\000"
	align	 8
@LC40:
	string	 "w\000"
	align	 8
@LC41:
	string	 "w\000"
	align	 8
@LC42:
	string	 "-traditional\000"
	align	 8
@LC43:
	string	 "-trigraphs\000"
	align	 8
@LC44:
	string	 "-lang-c\000"
	align	 8
@LC45:
	string	 "-lang-c++\000"
	align	 8
@LC46:
	string	 "-lang-objc\000"
	align	 8
@LC47:
	string	 "-lang-objc++\000"
	align	 8
@LC48:
	string	 "-lang-asm\000"
	align	 8
@LC49:
	string	 "-lint\000"
	align	 8
@LC50:
	string	 "-Wtrigraphs\000"
	align	 8
@LC51:
	string	 "-Wno-trigraphs\000"
	align	 8
@LC52:
	string	 "-Wcomment\000"
	align	 8
@LC53:
	string	 "-Wno-comment\000"
	align	 8
@LC54:
	string	 "-Wcomments\000"
	align	 8
@LC55:
	string	 "-Wno-comments\000"
	align	 8
@LC56:
	string	 "-Wtraditional\000"
	align	 8
@LC57:
	string	 "-Wno-traditional\000"
	align	 8
@LC58:
	string	 "-Wimport\000"
	align	 8
@LC59:
	string	 "-Wno-import\000"
	align	 8
@LC60:
	string	 "-Werror\000"
	align	 8
@LC61:
	string	 "-Wno-error\000"
	align	 8
@LC62:
	string	 "-Wall\000"
	align	 8
@LC63:
	string	 "-M\000"
	align	 8
@LC64:
	string	 "-MM\000"
	align	 8
@LC65:
	string	 "-MD\000"
	align	 8
@LC66:
	string	 "-MMD\000"
	align	 8
@LC67:
	string	 "-MD\000"
	align	 8
@LC68:
	string	 "-MMD\000"
	align	 8
@LC69:
	string	 "GNU CPP version %s\000"
	align	 8
@LC70:
	string	 "\n\000"
	align	 8
@LC71:
	string	 "Macro name missing after -D option\000"
	align	 8
@LC72:
	string	 "Assertion missing after -A option\000"
	align	 8
@LC73:
	string	 "-\000"
	align	 8
@LC74:
	string	 "-A\000"
	align	 8
@LC75:
	string	 "Macro name missing after -U option\000"
	align	 8
@LC76:
	string	 "-\000"
	align	 8
@LC77:
	string	 "Directory name missing after -I option\000"
	align	 8
@LC78:
	string	 "-nostdinc\000"
	align	 8
@LC79:
	string	 "-nostdinc++\000"
	align	 8
@LC80:
	string	 "-noprecomp\000"
	align	 8
@LC81:
	string	 "\000"
	align	 8
@LC82:
	string	 "\000"
	align	 8
@LC83:
	string	 "Invalid option `%s'\000"
	align	 8
@LC84:
	string	 "CPATH\000"
	align	 8
@LC85:
	string	 "\000"
