@YELP_HELP_RULES@

HELP_ID = mate-terminal
HELP_FILES =                                \
    index.page                              \
    legal.xml                               \
    mate-terminal-contents.page             \
    mate-terminal-encoding.page             \
    mate-terminal-first-start.page          \
    mate-terminal-introduction.page         \
    mate-terminal-manage-profiles.page      \
    mate-terminal-more-windows.page         \
    mate-terminal-prefs-colors.page         \
    mate-terminal-prefs-compatibility.page  \
    mate-terminal-prefs-effects.page        \
    mate-terminal-prefs-general.page        \
    mate-terminal-prefs-scrolling.page      \
    mate-terminal-prefs-title.page          \
    mate-terminal-profiles.page             \
    mate-terminal-reset.page                \
    mate-terminal-resize.page               \
    mate-terminal-shortcuts.page            \
    mate-terminal-tabs.page                 \
    mate-terminal-title.page                \
    mate-terminal-to-start.page             \
    mate-terminal-windows.page

HELP_MEDIA = figures/mate-terminal-default.png \
             figures/mate-terminal-tabbed.png 
#	      figures/terminal-window.png   This image is not used.

# Add linguas to be ignored, e.g. IGNORE_HELP_LINGUAS = ca de es fr
IGNORE_HELP_LINGUAS =
HELP_LINGUAS = $(if $(IGNORE_HELP_LINGUAS), \
	$(filter-out $(IGNORE_HELP_LINGUAS),$(subst /,,$(dir $(wildcard */*.po)))), \
	$(subst /,,$(dir $(wildcard */*.po))) )

-include $(top_srcdir)/git.mk
