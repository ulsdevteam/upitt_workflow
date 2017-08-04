<?php

/**
 * @file
 * Drush hooks.
 */

/**
 * Implements hook_drush_command().
 */
function upitt_workflow_drush_command() {
  $items = array();

  $items['upitt_workflow_generate_ocr_datastreams'] = array(
    'aliases' => array('uwgod'),
    'description' => 'Create OCR datastreams for paged content object\'s pages.',
    'drupal dependencies' => array('islandora_batch','islandora_ocr'),
    'examples' => array(
      'drush -v --user=admin --uri=http://digital.library.yorku.ca upitt_workflow_generate_ocr_datastreams --paged_object_pid=pitt:00awc1789m',
    ),
    'options' => array(
      'paged_object_pid' => array(
        'description' => 'The PID value of the paged content object.',
        'required' => TRUE,
      ),
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
  );
  return $items;
}

/**
 * Implements hook_islandora_batch_ingest().
 *
 * Ingests for the specified amount of time, if there is anything already
 * preprocessed.
 */
function drush_upitt_workflow_generate_ocr_datastreams() {
  module_load_include('inc', 'islandora', 'includes/derivatives');
  $parameters = array(
    'paged_object_pid' => drush_get_option('paged_object_pid', NULL),
  );

  $parameters = array_filter($parameters);

  $paged_content = islandora_object_load($parameters['paged_object_pid']);
  if (!$paged_content) {
    drush_print('Object ' . $parameters['paged_object_pid'] . ' does not exist');
    return;
  }
  islandora_job_submit_background('upitt_workflow_generate_ocr_datastreams', $paged_content->id, array());
}
