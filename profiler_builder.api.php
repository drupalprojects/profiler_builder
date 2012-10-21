<?php
/**
 * Implements hook_profiler_builder_modules_list_alter().
 */
function hook_profiler_builder_modules_list_alter(&$modules) {
  // remove the cdn module
  unset($modules['cdn']);
}

/**
 * Implements hook_profiler_builder_ignore_alter().
 */
function hook_profiler_builder_ignore_alter(&$ignore) {
  // ignore the cdn settings
  $ignore[] = 'cdn_settings';
}

/**
 * Implements hook_profiler_builder_variables_alter().
 */
function hook_profiler_builder_variables_alter(&$variables) {
  // variable values to change
  $change = array(
    'cdn_status' => 0,
  );
  // loop through and change only set values
  foreach ($change as $var => $val) {
    if (isset($variables[$var])) {
      $variables[$var] = $val;
    }
  }
}
?>