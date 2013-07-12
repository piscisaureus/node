{
  'targets': [
    {
      'target_name': 'openshift',
      'type': 'static_library',
      'include_dirs': [ '.' ],
      'sources': [ 'openshift.c' ],
      'direct_dependent_settings': {
        'include_dirs': [ '.' ]
      }
    }
  ] # end targets
}

