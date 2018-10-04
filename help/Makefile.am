@YELP_HELP_RULES@

HELP_ID = mate-terminal
HELP_FILES = index.docbook legal.xml
HELP_MEDIA = figures/mate-terminal-default.png \
             figures/mate-terminal-tabbed.png 
#	      figures/terminal-window.png   This image is not used.

# Add linguas to be ignored, e.g. IGNORE_HELP_LINGUAS = ca de es fr
IGNORE_HELP_LINGUAS =
HELP_LINGUAS = $(if $(IGNORE_HELP_LINGUAS), \
	$(filter-out $(IGNORE_HELP_LINGUAS),$(subst /,,$(dir $(wildcard */*.po)))), \
	$(subst /,,$(dir $(wildcard */*.po))) )

-include $(top_srcdir)/git.mk
