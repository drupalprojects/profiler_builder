<?php
/**
 * Implements hook_settings().
 */
function _profiler_builder_settings() {
  drupal_add_js(drupal_get_path('module', 'profiler_builder') . '/js/profiler_builder.js');
  $form = array();
  $form['profile_details'] = array(
    '#description' => t('Details of the installation profile to build'),
    '#collapsed' => FALSE,
    '#type' => 'fieldset',
    '#collapsible' => FALSE,
    '#title' => t('Profile details'),
  );
  $form['profile_details']['name'] = array(
    '#title' => t('Name'),
    '#description' => t('Example: Studio Machine') . ' (' . t('Do not begin name with numbers.') . ')',
    '#type' => 'textfield',
    '#attributes' => array('class' => array('profile-name')),
    '#field_suffix' => '<span class="field-suffix"></span>',
    '#required' => TRUE,
    '#default_value' => variable_get('site_name', ''),
  );
  $form['profile_details']['profile_machine_name'] = array(
    '#type' => 'machine_name',
    '#maxlength' => 32,
    '#machine_name' => array(
      'exists' => 'profiler_builder_get_info',
    ),
    '#description' => t('Example: studio_machine') . '<br/>' . t('May only contain lowercase letters, numbers and underscores. <strong>Try to avoid conflicts with the names of existing Drupal projects.</strong>'),
    '#required' => TRUE,
  );
  $form['profile_details']['description'] = array(
    '#title' => t('Description'),
    '#description' => t('Provide a short description of what users should expect when they use this profile.'),
    '#type' => 'textfield',
    '#default_value' => variable_get('site_slogan', ''),
  );
  $form['profile_details']['exclusive'] = array(
    '#title' => t('Exclusive'),
    '#description' => t('Treat this installation profile as exclusive? See the !url for more details about this setting.', array('!url' => l('related drupal.org discussion', 'http://drupal.org/node/1727430'))),
    '#type' => 'checkbox',
    '#default_value' => TRUE,
  );
  $form['profile_details']['create_admin'] = array(
    '#title' => t('Create an admin role'),
    '#description' => t('This creates an administrator role during installation. This is recommended as per the standard install profile but disable it if you are handling roles via Features / Ctools exports.'),
    '#type' => 'checkbox',
    '#default_value' => TRUE,
  );
  $form['profile_details']['standard_profile'] = array(
    '#title' => t('Create defaults like standard profile'),
    '#description' => t('This establishes the Page content type, RDF components, image field and associated taxonomy vocabulary Tags that were present in the "standard" drupal installation profile.'),
    '#type' => 'checkbox',
    '#default_value' => FALSE,
  );
  $form['drush_details'] = array(
    '#description' => t('Details of the drush make file to build'),
    '#collapsed' => FALSE,
    '#type' => 'fieldset',
    '#collapsible' => FALSE,
    '#title' => t('Drush make details'),
  );
  $form['drush_details']['local_dev'] = array(
    '#title' => t('Make file for local development'),
    '#description' => t('Check to build for local development, otherwise it will use the drupal.org distribution packaging format (minor difference).'),
    '#type' => 'checkbox',
    '#default_value' => TRUE,
  );
  $form['buttons']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Download profile'),
    '#weight' => 10,
    '#submit' => array('_profiler_builder_settings_form_submit'),
  );
  return $form;
}

/**
 * Form submission handler for profiler_builder_export_form().
 */
function _profiler_builder_settings_form_submit($form, &$form_state) {
  $name = $form_state['values']['name'];
  $machine_name = $form_state['values']['profile_machine_name'];
  $description = $form_state['values']['description'];
  $exclusive = $form_state['values']['exclusive'];
  // check if we should use standard profile setup routine
  if ($form_state['values']['standard_profile']) {
    $standard_profile_default = _profiler_builder_standard_profile_default();
  }
  else {
    $standard_profile_default = '';
  }
  // check if we should create an admin role
  if ($form_state['values']['create_admin']) {
    $create_admin = _profiler_builder_create_admin();
  }
  else {
    $create_admin = '';
  }
  
  // drush setting for drupal.org distro or local
  $local_dev = $form_state['values']['local_dev'];
  if ($local_dev) {
    $drush_file_name = 'drush.make';
  }
  else {
    $drush_file_name = 'drupal-org.make';
  }
  // assemble profiler components
  $features = _profiler_builder_components($info, $vars, $drush);
  // build initial info file
  $info_header = _profiler_builder_info_file($name, $machine_name, $description, $exclusive);
  // build files array
  $files = array(
    $machine_name . '.info' => $info_header . $info . $vars,
    $machine_name . '.install' => _profiler_builder_install_file($machine_name, array_keys($features), $standard_profile_default, $create_admin),
    $machine_name . '.profile' => _profiler_builder_profile_file($name, $machine_name),
    $drush_file_name => _profiler_builder_drush_file($drush, $name, $local_dev),
  );
  // Generate download file name
  $filename =  $machine_name . '.tar';
  // Clear out output buffer to remove any garbage from tar output.
  if (ob_get_level()) {
    ob_end_clean();
  }
  // set headers for file download on submit
  drupal_add_http_header('Content-type', 'application/x-tar');
  drupal_add_http_header('Content-Disposition', 'attachment; filename="'. $filename .'"');
  drupal_send_headers();
  // loop through file array and add to package
  foreach ($files as $file_name => $file_contents) {
    // write file to tar package
    print profiler_builder_tar_create("{$machine_name}/{$file_name}", $file_contents);
  }
  // end tar file output
  print pack("a1024","");
  exit;
}

/**
 * Helper function to create .install file
 */
function _profiler_builder_install_file($machine_name, $features, $standard_profile_default, $create_admin) {
  $output = '<?php
  !function_exists(\'profiler_v2\') ? require_once(\'libraries/profiler/profiler.inc\') : FALSE;
  profiler_v2(\''. $machine_name .'\');

/**
 * Implements hook_install().
 *
 * Perform actions to set up the site for this profile.
 */
function '. $machine_name .'_install() {
  // Enable some '. $machine_name .' blocks.
  $default_theme = \''. variable_get('theme_default', 'bartik') .'\';
  $admin_theme = \''. variable_get('admin_theme', 'seven') .'\';
  // disable all themes
  db_update(\'system\')
    ->fields(array(\'status\' => 0))
    ->condition(\'type\', \'theme\')
    ->execute();
  // enable $default_theme 
  db_update(\'system\')
    ->fields(array(\'status\' => 1))
    ->condition(\'type\', \'theme\')
    ->condition(\'name\', $default_theme)
    ->execute();
  // enable $admin_theme
  db_update(\'system\')
    ->fields(array(\'status\' => 1))
    ->condition(\'type\', \'theme\')
    ->condition(\'name\', $admin_theme)
    ->execute();
  variable_set(\'admin_theme\', $admin_theme);
  // activate admin theme when editing a node
  variable_set(\'node_admin_theme\', \'1\');
  $blocks = array(
    array(
      \'module\' => \'system\',
      \'delta\' => \'main\',
      \'theme\' => $default_theme,
      \'status\' => 1,
      \'weight\' => 0,
      \'region\' => \'content\',
      \'pages\' => \'\',
      \'cache\' => -1,
    ),
    array(
      \'module\' => \'user\',
      \'delta\' => \'login\',
      \'theme\' => $default_theme,
      \'status\' => 1,
      \'weight\' => 0,
      \'region\' => \'sidebar_first\',
      \'pages\' => \'\',
      \'cache\' => -1,
    ),
    array(
      \'module\' => \'system\',
      \'delta\' => \'help\',
      \'theme\' => $default_theme,
      \'status\' => 1,
      \'weight\' => -10,
      \'region\' => \'content\',
      \'pages\' => \'\',
      \'cache\' => -1,
    ),
    array(
      \'module\' => \'system\',
      \'delta\' => \'main\',
      \'theme\' => $admin_theme,
      \'status\' => 1,
      \'weight\' => 0,
      \'region\' => \'content\',
      \'pages\' => \'\',
      \'cache\' => -1,
    ),
    array(
      \'module\' => \'system\',
      \'delta\' => \'help\',
      \'theme\' => $admin_theme,
      \'status\' => 1,
      \'weight\' => 0,
      \'region\' => \'help\',
      \'pages\' => \'\',
      \'cache\' => -1,
    ),
  );
  // drop system / user blocks to ensure correct building
  db_delete(\'block\')->condition(\'module\', \'system\')->execute();
  db_delete(\'block\')->condition(\'module\', \'user\')->execute();
  // add in our blocks defined above
  $query = db_insert(\'block\')->fields(array(\'module\', \'delta\', \'theme\', \'status\', \'weight\', \'region\', \'pages\', \'cache\'));
  foreach ($blocks as $block) {
    $query->values($block);
  }
  $query->execute();
  '. $standard_profile_default .'
  // revert features to ensure they are all installed
  $features = array(
    \''. implode("',\n    '", $features) .'\'
  );
  features_revert($features);

  // From shortcut.install, add shortcuts to the default set on install
  $shortcut_set = shortcut_set_load(SHORTCUT_DEFAULT_SET_NAME);
  $shortcut_set->links = NULL;
  $shortcut_set->links = array(
    array(
      \'link_path\' => \'admin/appearance/settings\', 
      \'link_title\' => st(\'Theme\'), 
      \'weight\' => -17,
    ),
  );
  shortcut_set_save($shortcut_set);
  '. $create_admin .'
  // Update the menu router information.
  menu_rebuild();
  // ignore any rebuild messages
  node_access_needs_rebuild(FALSE);
  // ignore any other install messages
  drupal_get_messages();
}';
  return $output;
}

/**
 * Helper function to create .info file
 */
function _profiler_builder_info_file($name, $machine_name, $description, $exclusive) {
  $active_core = PROIFLER_BUILDER_ACTIVE_CORE;
  $output = "; Packaging information for $machine_name\n";
  $output .= "name = \"$name\"\n";
  $output .= "description = \"$description\"\n";
  // if exclusive flag set, include it
  if ($exclusive) {
    $output .= "exclusive = \"$exclusive\"\n";
  }
  $output .= "core = \"$active_core.x\"\n";
  return $output;
}

/**
 * Helper function to create .profile file
 */
function _profiler_builder_profile_file($name, $machine_name) {
  // assemble default output
  $output = '<?php
/**
 * Implements hook_form_FORM_ID_alter().
 *
 * Allows the profile to alter the site configuration form.
 */
function system_form_install_configure_form_alter(&$form, $form_state) {
  // Pre-populate the site name with the server name.
  $form[\'site_information\'][\'site_name\'][\'#default_value\'] = \''. $name .'\';
}
/**
 * Implements hook_form_alter().
 */
function system_form_install_select_profile_form_alter(&$form, $form_state) {
  // select '. $machine_name .' install profile by default
  foreach($form[\'profile\'] as $key => $element) {
    $form[\'profile\'][$key][\'#value\'] = \''. $machine_name .'\';
  }
}';
  return $output;
}

/**
 * Helper to make .make file.
 */
function _profiler_builder_drush_file($file_contents, $name, $local) {
  $active_core = PROIFLER_BUILDER_ACTIVE_CORE;
  $output = "; $name make file\n";
  $output .= "core = \"$active_core.x\"\n";
  $output .= "api = \"2\"\n";
  // change file based on local development or d.o.
  if ($local) {
    $output .= "; comment this out in to use on drupal.org\n";
    $output .= "projects[drupal][version] = \"$active_core.x\"\n";
  }
  else {
    $output .= "; comment this in to use in local development\n";
    $output .= "; projects[drupal][version] = \"$active_core.x\"\n";
  }
  return $output . $file_contents;
}

/**
 * Helper to return standard profile default settings.
 */
function _profiler_builder_standard_profile_default() {
  $output = '
  // Insert default pre-defined node types into the database. For a complete
  // list of available node type attributes, refer to the node type API
  // documentation at: http://api.drupal.org/api/HEAD/function/hook_node_info.
  $types = array(
    array(
      \'type\' => \'page\',
      \'name\' => st(\'Page\'),
      \'base\' => \'node_content\',
      \'description\' => st("Use <em>pages</em> for basic page content."),
      \'custom\' => 1,
      \'modified\' => 1,
      \'locked\' => 0,
    ),
  );

  foreach ($types as $type) {
    $type = node_type_set_defaults($type);
    node_type_save($type);
    node_add_body_field($type);
  }

  // Insert default pre-defined RDF mapping into the database.
  $rdf_mappings = array(
    array(
      \'type\' => \'node\',
      \'bundle\' => \'page\',
      \'mapping\' => array(
        \'rdftype\' => array(\'foaf:Document\'),
      ),
    ),
  );
  foreach ($rdf_mappings as $rdf_mapping) {
    rdf_mapping_save($rdf_mapping);
  }

  // Default "Page" to not be promoted and have comments disabled.
  variable_set(\'node_options_page\', array(\'status\'));

  // Don\'t display date and author information for "Page" nodes by default.
  variable_set(\'node_submitted_page\', FALSE);

  // Create a default vocabulary named "Tags", enabled for the \'article\' content type.
  $description = st(\'Use tags to group articles on similar topics into categories.\');
  $help = st(\'Enter a comma-separated list of words to describe your content.\');
  $vocabulary = (object) array(
    \'name\' => st(\'Tags\'),
    \'description\' => $description,
    \'machine_name\' => \'tags\',
    \'help\' => $help,

  );
  taxonomy_vocabulary_save($vocabulary);

  $field = array(
    \'field_name\' => \'field_\' . $vocabulary->machine_name,
    \'type\' => \'taxonomy_term_reference\',
    // Set cardinality to unlimited for tagging.
    \'cardinality\' => FIELD_CARDINALITY_UNLIMITED,
    \'settings\' => array(
      \'allowed_values\' => array(
        array(
          \'vocabulary\' => $vocabulary->machine_name,
          \'parent\' => 0,
        ),
      ),
    ),
  );
  field_create_field($field);

  $instance = array(
    \'field_name\' => \'field_\' . $vocabulary->machine_name,
    \'entity_type\' => \'node\',
    \'label\' => \'Tags\',
    \'bundle\' => \'article\',
    \'description\' => $vocabulary->help,
    \'widget\' => array(
      \'type\' => \'taxonomy_autocomplete\',
      \'weight\' => -4,
    ),
    \'display\' => array(
      \'default\' => array(
        \'type\' => \'taxonomy_term_reference_link\',
        \'weight\' => 10,
      ),
      \'teaser\' => array(
        \'type\' => \'taxonomy_term_reference_link\',
        \'weight\' => 10,
      ),
    ),
  );
  field_create_instance($instance);


  // Create an image field named "Image", enabled for the \'article\' content type.
  // Many of the following values will be defaulted, they\'re included here as an illustrative examples.
  // See http://api.drupal.org/api/function/field_create_field/'. PROIFLER_BUILDER_ACTIVE_CORE .'

  $field = array(
    \'field_name\' => \'field_image\',
    \'type\' => \'image\',
    \'cardinality\' => 1,
    \'locked\' => FALSE,
    \'indexes\' => array(\'fid\' => array(\'fid\')),
    \'settings\' => array(
      \'uri_scheme\' => \'public\',
      \'default_image\' => FALSE,
    ),
    \'storage\' => array(
      \'type\' => \'field_sql_storage\',
      \'settings\' => array(),
    ),
  );
  field_create_field($field);


  // Many of the following values will be defaulted, they\'re included here as an illustrative examples.
  // See http://api.drupal.org/api/function/field_create_instance/'. PROIFLER_BUILDER_ACTIVE_CORE .'
  $instance = array(
    \'field_name\' => \'field_image\',
    \'entity_type\' => \'node\',
    \'label\' => \'Image\',
    \'bundle\' => \'article\',
    \'description\' => st(\'Upload an image to go with this article.\'),
    \'required\' => FALSE,

    \'settings\' => array(
      \'file_directory\' => \'field/image\',
      \'file_extensions\' => \'png gif jpg jpeg\',
      \'max_filesize\' => \'\',
      \'max_resolution\' => \'\',
      \'min_resolution\' => \'\',
      \'alt_field\' => TRUE,
      \'title_field\' => \'\',
    ),

    \'widget\' => array(
      \'type\' => \'image_image\',
      \'settings\' => array(
        \'progress_indicator\' => \'throbber\',
        \'preview_image_style\' => \'thumbnail\',
      ),
      \'weight\' => -1,
    ),

    \'display\' => array(
      \'default\' => array(
        \'label\' => \'hidden\',
        \'type\' => \'image\',
        \'settings\' => array(\'image_style\' => \'large\', \'image_link\' => \'\'),
        \'weight\' => -1,
      ),
      \'teaser\' => array(
        \'label\' => \'hidden\',
        \'type\' => \'image\',
        \'settings\' => array(\'image_style\' => \'medium\', \'image_link\' => \'content\'),
        \'weight\' => -1,
      ),
    ),
  );
  field_create_instance($instance);
  // Create a Home link in the main menu.
  $item = array(
    \'link_title\' => st(\'Home\'),
    \'link_path\' => \'<front>\',
    \'menu_name\' => \'main-menu\',
  );
  menu_link_save($item);';
  return $output;
}

/**
 * Helper function to create an admin in the .install file.
 */
function _profiler_builder_create_admin() {
  $output = '// Create a default role for site administrators, with all available permissions assigned.
  $admin_role = new stdClass();
  $admin_role->name = \'administrator\';
  $admin_role->weight = 10;
  user_role_save($admin_role);
  user_role_grant_permissions($admin_role->rid, array_keys(module_invoke_all(\'permission\')));
  // Set this as the administrator role.
  variable_set(\'user_admin_role\', $admin_role->rid);

  // Assign user 1 the "administrator" role.
  db_insert(\'users_roles\')
    ->fields(array(\'uid\' => 1, \'rid\' => $admin_role->rid))
    ->execute();';
  return $output;
}