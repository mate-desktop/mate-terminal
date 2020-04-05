libskey = static_library(
  'skey',
  sources : [
	'btoe.c',
	'btoe.h',
	'skey.h',
	'skey.c',
	'skeyutil.h',
	'skeyutil.c',
	'md4.c',
	'md4.h',
	'md5.c',
	'md5.h',
	'sha1.h',
	'sha1.c',
  ],
  dependencies : term_deps,
  install: false,
)

t=executable('testskey',
  'test.c',
  link_with : [libskey],
  dependencies : term_deps,
)

test('test skey', t)
