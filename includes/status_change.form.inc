<?php

function upitt_workflow_status_change(array $form, array &$form_state) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  $action_options = upitt_workflow_get_mysql_options('action', 'Set Action', 'name', 'description', 'name', '0');

  $autofocus_attribute = array('autofocus' => 'autofocus');
  if (isset($form_state['input']['action'])) {
    $_SESSION['action'] = $form_state['input']['action'];
    $action = $form_state['input']['action'];
  }
  else {
    $action = @$_SESSION['action'];
  }

  $form = array(
    'breadcrumb' => array('#markup' => upitt_workflow_get_breadcrumb_path()),
    'nid' => array(
      '#type' => 'hidden',
      '#default_value' => $nid,
    ),
    'q' => array(
      '#type' => 'textfield',
      '#title' => t('Look up an item by id'),
      '#attributes' => $autofocus_attribute,
      '#default_value' => '',
    ),
    
    'action' => array(
      '#type' => 'select',
      '#title' => t('Set Action'),
      '#options' => $action_options,
      '#default_value' => $action,
      '#size' => 14,
    ),

    'submit' => array(
      '#type' => 'submit',
      '#value' => t('Update'),
    ),
  );
  return $form;
}

function upitt_workflow_status_change_submit(array $form, array &$form_state) {
  global $user;
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $object_id = $form_state['values']['q'];
  $action = $form_state['values']['action'];
  $action_string = (array_key_exists($action, $form['action']['#options'])) ? $form['action']['#options'][$action] : '';

  if ($action && $action_string) {
    upitt_workflow_update_object_status($object_id, $action);
    drupal_set_message('Set <b>' . $action_string . '</b> status on ' . l($object_id, '/workflow/object/' . $object_id) . '.');
    // ONLY create a new drupal node if this record does not yet have a nid value.
    if (isset($form_state['values']['nid']) && $form_state['values']['nid'] > 0) {
      $node = node_load($form_state['values']['nid']);
    }
  }
  else {
    drupal_set_message('No action was set'. ($object_id ? '' : ' and there no Object ID was provided') . '.');
  }
//  $form_state['redirect'] = '/workflow/batch/' . $batch_id;
  return TRUE;
}
