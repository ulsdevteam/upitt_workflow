<?php

/**
 * @file
 * Administration forms and submit handling for upitt_workflow module.
 */

/**
 * Administration and configuration form for upitt_workflow.
 *
 * @ingroup forms
 *
 * @todo break out non-settings elements into their own form.
 *
 * @return array
 *   The Drupal form definition.
 */
function upitt_workflow_admin_form() {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  $ingest_namespace = upitt_workflow_get_ingest_namespace();

  $password_field_description = (variable_get('upitt_workflow_ftp_password') ?
          '<span style="color:red">' . t('There is a value for the password, but it is not displayed.  
             Only enter text here if the password value needs to change.') . '</span>':
          t('There is NOT any password value.  FTP will not work without a valid password.'));
  $box_alias_default = 'box.com=ftp';
  $default_admin_only_process_barcode_modes = 'update' . "\n" .
    'updateforce' . "\n" .
    'gen_OCR' . "\n" .
    'solr_reindex' . "\n" .
    'purge_OCR_PDF' . "\n" .
    'delete_batch' . "\n" .
    'delete_batch_tempfiles' . "\n" .
    'purge_objects';
  $form = array(
    'notification_email' => array(
      '#type' => 'textfield',
      '#title' => t('Notification Email Address'),
      '#description' => t('The email addresses to send notifications of various workflow actions.  Separate multiple address values with semicolon.'),
      '#default_value' => variable_get('upitt_workflow_notification_email', 'digital-collections@pitt.edu'),
    ),
    'ingest_fields' => array(
      '#type' => 'fieldset',
      '#title' => 'Ingest options',
      'ingest_drush_debugmode' => array(
        '#type' => 'checkbox',
        '#title' => t("Drush Debug?"),
        '#default_value' => variable_get('upitt_workflow_ingest_drush_debugmode', FALSE),
        '#description' => t('When set, all drush commands will be passed with "debug" and "verbose" switches.'),
      ),
      'ingest_namespace' => array(
        '#type' => 'textfield',
        '#title' => t('Ingest Namespace'),
        '#description' => t('The namespace to use for ingesting all objects.'),
        '#default_value' => $ingest_namespace,
      ),
      'ingest_prepared_path' => array(
        '#type' => 'textfield',
        '#title' => t('Ingest Working Directory'), // Incoming Ingest Path'),
        '#description' => t('The working directory for prepared ingest batch files.  This path
                             should be configured to a location that has plenty of hard space.'),
        '#default_value' => variable_get('upitt_workflow_ingest_prepared_path', '/ingest/islandora_ingest'),
      ),
      'ingest_incoming_path' => array(
        '#type' => 'textfield',
        '#title' => t('Incoming Directories'),
        '#description' => t('The incoming batch files directories.  In order to be ingested, the folders
                             here should be named to match the name of the batch.  The ingest code will convert the files
                             and the structure of the contents for an Islandora batch.'),
        '#default_value' => variable_get('upitt_workflow_ingest_incoming', '/ingest/incoming'),
      ),
      'batch_hosts' => array(
        '#type' => 'textarea',
        '#title' => t('Available Host Aliases for Batch ingest'),
        '#default_value' => variable_get('upitt_workflow_hosts_for_ingest', $box_alias_default),
        '#description' => t('If the ingestable files are located on a different server, configure an alias
                             that can direct the batch to use that server.<br>
                             <ul>
                               <li>If you do not want to use box.com as a potential source for ingest files, simply remove the entry of <code>' . $box_alias_default . '</code></li>
                               <li>For any batch that does not specify which host alias to use, the "Default Incoming Ingest Path" above would be used.</li>
                               <li>Values should each be entered on separate lines in a "name=value" format.  Username, password values are optional.  For example:
                                 <ul>
                                   <li>The value (part after the "=" character) should not be longer than 255 characters.</li>
                                   <li><code>ALIAS=username:password@machine_name:/path</code></li>
                                   <li><code>ALIAS=machine_name:/path</code></li>
                                 </ul>
                               </li>
                             </ul>'),
      ),
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
      /* 'ftp_port' => array(
        '#type' => 'textfield',
        '#title' => t('FTP Port'),
        '#description' => t('21 default FTP, 990 default sFTP.'),
        '#default_value' => variable_get('upitt_workflow_ftp_port', 990),
      ), */
    ),
    'process_barcodes_panel' => array(
      '#type' => 'fieldset',
      '#title' => t('Process Barcodes Admin-only modes.'),
      'admin_only_process_barcode_modes' => array(
        '#type' => 'textarea',
        '#title' => t('List the mode values that ONLY administrators should have access to use.'),
        '#default_value' => variable_get('upitt_workflow_admin_only_process_barcode_modes', $default_admin_only_process_barcode_modes),
        '#description' => t('List the individual system names for the modes that should only be accessible by administrators.  These
                             are listed in the source code for the process_barcodes.form.inc.'),
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
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  $id = $form_state['triggering_element']['#id'];
  switch ($id) {
    case 'edit-submit':
      $batch_hosts_array = explode("\r\n", $form_state['values']['batch_hosts']);
      upitt_workflow_create_host_paths($batch_hosts_array);
      variable_set('upitt_workflow_admin_only_process_barcode_modes', $form_state['values']['admin_only_process_barcode_modes']);
      variable_set('upitt_workflow_notification_email', $form_state['values']['notification_email']);
      variable_set('upitt_workflow_ingest_namespace', $form_state['values']['ingest_namespace']);
      variable_set('upitt_workflow_ingest_prepared_path', $form_state['values']['ingest_prepared_path']);
      variable_set('upitt_workflow_ingest_incoming', $form_state['values']['ingest_incoming_path']);
      variable_set('upitt_workflow_ingest_drush_debugmode', $form_state['values']['ingest_drush_debugmode']);
      variable_set('upitt_workflow_ftp_username', $form_state['values']['ftp_username']);
      variable_set('upitt_workflow_ftp_password', $form_state['values']['ftp_password']);
      // variable_set('upitt_workflow_ftp_port', $form_state['values']['ftp_port']);
      variable_set('upitt_workflow_hosts_for_ingest', $form_state['values']['batch_hosts']);
      break;

    case 'edit-reset':
      variable_del('upitt_workflow_admin_only_process_barcode_modes');
      variable_del('upitt_workflow_notification_email');
      variable_del('upitt_workflow_ingest_namespace');
      variable_del('upitt_workflow_ingest_prepared_path');
      variable_del('upitt_workflow_ingest_incoming');
      variable_del('upitt_workflow_ingest_drush_debugmode');
      variable_del('upitt_workflow_ftp_username');
      variable_del('upitt_workflow_ftp_password');
      // variable_del('upitt_workflow_ftp_port');
      variable_del('upitt_workflow_hosts_for_ingest');
      break;
  }
  // Set this message after the folders may be created in case there are any error messages.
  drupal_set_message(t('The settings have been updated!'));
}
