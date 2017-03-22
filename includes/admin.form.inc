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
  $password_field_description = (variable_get('upitt_workflow_ftp_password') ?
          '<span style="color:red">There is a value for the password, but it is not displayed.  Only enter text here if the password value needs to change.</span>':
          'There is NOT any password value.  FTP will not work without a valid password.');
  $form = array(
    'incoming_ingest_path' => array(
      '#type' => 'textfield',
      '#title' => t('Incoming Ingest Path'),
      '#description' => t('The incoming ingest directory.  In order to be ingested, the folders created here should be named to match the name of the batch.  This path should be configured to a location that has plenty of hard space.'),
      '#default_value' => variable_get('upitt_workflow_ingest_path', '/ingest/incoming'),
      '#size' => 100,
    ),
    'ftp_panel' => array(
      '#type' => 'fieldset',
      '#title' => t('FTP settings for box.com account access.'),
      'ftp_username' => array(
        '#type' => 'textfield',
        '#title' => t('Username with FTP access'),
        '#default_value' => variable_get('upitt_workflow_ftp_username'),
      ),
      'ftp_password' => array(
        '#type' => 'password',
        '#title' => t('Password'),
        '#description' => $password_field_description,
      ),
      'ftp_port' => array(
        '#type' => 'textfield',
        '#title' => t('FTP Port'),
        '#description' => t('21 default FTP, 990 default sFTP.'),
        '#default_value' => variable_get('upitt_workflow_ftp_port', 990),
      ),
    ),
    'submit' => array(
      '#type' => 'submit',
      '#value' => t('Save configuration'),
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
      variable_set('upitt_workflow_ftp_username', $form_state['values']['ftp_username']);
      variable_set('upitt_workflow_ftp_password', $form_state['values']['ftp_password']);
      variable_set('upitt_workflow_ftp_port', $form_state['values']['ftp_port']);

      break;

    case 'edit-reset':
      variable_del('upitt_workflow_ingest_path');
      variable_del('upitt_workflow_ftp_username');
      variable_del('upitt_workflow_ftp_password');
      variable_del('upitt_workflow_ftp_port');
      break;
  }
}
