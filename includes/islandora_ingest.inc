<?php

/**
 * This function will check that all requirements for this ingest are provided -- and then it will prepare the workflow objects 
 * and generate the appropriate islandora_job batch call to ingest this to islandora using the Gearman process to do the work.
 *
 * The pid value is created by adding "pitt:" to the beginning of the object_id (legacy: item.do_id) value.
 *
 * Any required files that become datastreams can immediately be ingested.
 *
 * The fedora_model_name is used as part of the dynamic function naming -- 
 *   1. To create base object:       "upitt_ingest_jobs_{FEDORA_MODEL_NAME}_create_object"
 *   2. To add a given datastream:   "upitt_ingest_jobs_{FEDORA_MODEL_NAME}_add_datastream"
 *   3. To generate a derivative:    "upitt_ingest_jobs_{FEDORA_MODEL_NAME}_make_derivative"
 *   4. To add relationships?:       "upitt_ingest_jobs_{FEDORA_MODEL_NAME}_add_relationship"
 */
function upitt_workflow_islandora_ingest($object_id) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  module_load_include('inc', 'upitt_ingest_jobs', 'includes/utilities');
  $batch = upitt_workflow_batch_of_item($object_id);

  $markup = '<p>Ingest single item "' . $object_id . '"';
  
  if ($batch) {
    $workflow_type_to_fedoramodel_mapping = upitt_workflow_get_workflow_to_fedora_object_mappings();

    // translate the batch's object type to a fedora object type for the requirements:
    $fedora_model_name = $workflow_type_to_fedoramodel_mapping[$batch['default_type_of_resource']];

    $markup .= ' from batch ' . $batch['batch_external_id'] . '</p>';

    $markup .= '<h3 style="color:green">Batch `default_type_of_resource` = "' . $batch['default_type_of_resource'] . '", yields fedora model: "' . $fedora_model_name . '"</h3><br>';

    $requirements = upitt_ingest_jobs_fedora_object_requirements($fedora_model_name);
    if ($requirements) {
      $markup  .= '<h3>Requirements for "' . $fedora_model_name . '"</h3><pre>' . print_r($requirements, true) . '</pre>';
    }

    $use_pid = 'pitt:' . $object_id;

    $item_files = upitt_workflow_get_item_files($object_id);
    if ($item_files) {
      $markup  .= '<h3>Item Files for "' . $object_id . '"</h3>';
      foreach ($item_files as $item_file) {
        $exists_color = (file_exists($item_file['path']) ? 'green' : 'red');
        // Is this file one of the required files?  That could potentially be batched out ez...

        $markup .= '<pre style="color:' . $exists_color . '">' . 
          '<b>id</b>: ' . $item_file['id'] . '<br>' .
          '<b>item_id</b>: ' . $item_file['item_id'] . '<br>' .
          '<b>name</b>: ' . $item_file['name'] . '<br>' .
          '<b>path</b>: ' . $item_file['path'] . '<br>' .
          '<b>host</b>: ' . $item_file['host'] . '<br>' .
          '<b>size_bytes</b>: ' . $item_file['size_bytes'] . '<br>' .
          '<b>mime_type</b>: ' . $item_file['mime_type'] . '<br>' .
          '<b>use</b>: ' . $item_file['use'] . '<br>' .
          '<b>timestamp</b>: ' . $item_file['timestamp'] . '<br></pre>';
      }
      $islandora_object = islandora_object_load($use_pid);
      if (is_object($islandora_object)) {
        drupal_set_message(t('This object already exists.  The create job function has not been called.'), 'warning');
      }
      else {
        // create object job call
        $function_name = 'upitt_ingest_jobs_' . $fedora_model_name . '_create_object';
        $markup .= "<hr>Create object function call: <code>" . $function_name . "(\$pid = '" . $use_pid . "');</code> (" . ((function_exists($function_name)) ? 'exists' : 'no function') . ')';
        if (function_exists($function_name)) {
          $function_name($use_pid);
        }
      }
      
      // make derivatives job calls 
      $function_name = 'upitt_ingest_jobs_' . $fedora_model_name . '_make_derivative';
      foreach ($requirements['islandora_create_derivatives'] as $derivative_requirement) {
        $markup .= "<hr>Create derivatives function call: <code>" . $function_name . "(\$pid = '" . $use_pid . "', \$dsid = '" . $derivative_requirement . "');</code> " . 
                   "(" . ((function_exists($function_name)) ? 'exists' : 'no function') . ')';
        if (function_exists($function_name)) {
          $function_name($use_pid, $derivative_requirement);
        }
      }

      // passthrough datastream job calls
      $function_name = 'upitt_ingest_jobs_' . $fedora_model_name . '_add_datastream';
      foreach ($requirements['passthrough_datastreams'] as $passthrough_datastream_requirement) {
        $source_key = array_shift(array_keys($passthrough_datastream_requirement));
        $passthrough_source = upitt_workflow_get_item_file_for_specific_use($source_key, $item_files);
        if ($passthrough_source) {
          $datastream_target = $passthrough_source['path'];
          $markup .= "<hr>Passthrough derivative function call: <code>" . $function_name . "(\$pid = '" . $use_pid . "', " . 
                     "\$dsid = '" . $passthrough_datastream_requirement . "', \$datastream_source = '" . $datastream_source . "');</code> (" . ((function_exists($function_name)) ? 'exists' : 'no function') . ')';
          if (function_exists($function_name)) {
            $function_name($use_pid, $passthrough_datastream_requirement, $datastream_source);
          }
        }
        else {
          drupal_set_message(t('The passthrough datastream for ' . $source_key . ' could not be found in this object\'s records'), 'warning');
        }
      }
    }
    if (upitt_workflow_validate_ingest_for_object($item_files, $requirements)) {
      $markup .= '<h3 style="color: green">Requirements all satisfied</h3>';      
    } else {
      $markup .= '<h3 style="color: red">Requirements not satisfied</h3>';
    }
    $markup .= '<h3>Batch values</h3>';
    $markup .= theme('workflow_item_details', array('details' => $batch));
  } else {
    $markup .= ', but the batch record could not be found in the workflow for this object.</p>';
  }

  return $markup;
}

/**
 * This function will return the specific item file record that corresponds to a given `use` value, like the `TARGET` reference 
 * would be whichever item_file record that had that value.
 *
 * NOTE: There should only be one item_file reference for each of the `use` values that the code which calls this - since it has
 * been configured such that only one datastream will be created from it.
 */
function upitt_workflow_get_item_file_for_specific_use($use, $item_files) {
  foreach ($item_files as $item_file) {
    if ($item_file['use'] == $use) {
      return $item_file;
    }
  }
  return NULL;
}

/**
 * This will inspect the current workflow object for ingest -- and the object type's requirements and will return 
 * TRUE if the object can be ingested (all requirements met).  If there are any failure reasons, the function will 
 * set the appropriate drupal_set_message.
 */
function upitt_workflow_validate_ingest_for_object($item_files, $requirements) {

  return FALSE;
}
