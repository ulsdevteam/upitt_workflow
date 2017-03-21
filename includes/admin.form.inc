<?php

/**
 * @file
 * Administration forms and submit handling for isMemberOfSite module.
 */

/**
 * Administration and configuration form for memberofsite.
 *
 * @ingroup forms
 *
 * @todo break out non-settings elements into their own form.
 *
 * @return array
 *   The Drupal form definition.
 */
function upitt_workflow_admin_form() {
  $form = array(
    'incoming_ingest_path' => array(
      '#type' => 'textfield',
      '#title' => t('Incoming Ingest Path'),
      '#description' => t('The incoming ingest directory.  In order to be ingested, the folders created here should be named to match the name of the batch.  This path should be configured to a location that has plenty of hard space.'),
      '#default_value' => variable_get('upitt_workflow_ingest_path', '/ingest/incoming'),
      '#size' => 100,
    ),
    'exports_ingest_path' => array(
      '#type' => 'textfield',
      '#title' => t('Exports Path'),
      '#description' => t('This directory is where exported datastreams would be saved in a folder corresponding to the batch identifier, the date, and the datastream identifier.'),
      '#default_value' => variable_get('upitt_workflow_export_path', '/ingest/tmp'),
      '#size' => 100,
    ),
    'submit' => array(
      '#type' => 'submit',
      '#value' => t('Save configuration'),
    ),
    'reset' => array(
      '#type' => 'submit',
      '#value' => t('Reset to defaults'),
    ),
  );
  return $form;
}

/**
 * Function that sets the Drupal variables with user's input.
 *
 * @param array $form
 *   The Drupal form definition.
 * @param array $form_state
 *   The Drupal form state.
 */
function upitt_workflow_admin_form_submit(array $form, array &$form_state) {
  drupal_set_message(t('The settings have been updated!'));
  $id = $form_state['triggering_element']['#id'];
  switch ($id) {
    case 'edit-submit':
      variable_set('upitt_workflow_ingest_path', $form_state['values']['incoming_ingest_path']);
      variable_set('upitt_workflow_export_path', $form_state['values']['exports_ingest_path']);
      break;

    case 'edit-reset':
      variable_del('upitt_workflow_ingest_path');
      variable_del('upitt_workflow_export_path');
      break;
  }
}
