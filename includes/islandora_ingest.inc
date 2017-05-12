<?php

function upitt_workflow_islandora_object_ingest($object_id) {
  drupal_set_message('ingest by object not currently supported');
  // This code would need to use the upitt_workflow_batch_of_item function to determine
  // the batch of the given object id
}

/**
 * This function will check that all requirements for this ingest are provided -- and then it will prepare the workflow objects
 * and generate the appropriate islandora_job batch call to ingest this to islandora using the Gearman process to do the work.
 *
 * The pid value is created by adding "pitt:" to the beginning of the object_id (legacy: item.do_id) value.
 *
 * Any required files that become datastreams can immediately be ingested.
 *
 * The fedora_model_name is used as part of the dynamic function naming --
 *   1. To create base object:       "upitt_workflow_{FEDORA_MODEL_NAME}_create_object"
 *   2. To add a given datastream:   "upitt_workflow_{FEDORA_MODEL_NAME}_add_datastream"
 *   3. To generate a derivative:    "upitt_workflow_{FEDORA_MODEL_NAME}_make_derivative"
 *   4. To add relationships?:       "upitt_workflow_{FEDORA_MODEL_NAME}_add_relationship"
 */
function upitt_workflow_islandora_ingest($batch_id) {
  global $user;

  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  $batch = upitt_workflow_batch_load($batch_id);

  $markup = '<p>Attempt to ingest batch #' . $batch_id;

  if ($batch) {
    $markup .= ', "' . $batch['batch_external_id'] . '"';

    $fedora_model_name = $batch['default_type_of_resource'];

    $markup .= ' from batch ' . $batch['batch_external_id'] . '</p>';

    $markup .= '<h3 style="color:green">Batch `Model` (default_type_of_resource) = "' . $batch['default_type_of_resource'] . '", yields fedora model: "' . $fedora_model_name . '"</h3><br>';

    /* // bgg - skip this -
    $requirements = upitt_workflow_fedora_object_requirements($fedora_model_name);
    if ($requirements) {
      $markup  .= '<h3>Requirements for "' . $fedora_model_name . '"</h3><pre>' . print_r($requirements, true) . '</pre>';
    } */

    $batch_path = upitt_workflow_batch_path($batch);

    // For the drush call, we need to get a few values from the batch record and even lookup the collection PIDs and item_file path.
//    $parent = upitt_workflow_get_object_fedora_collections($object_id);
//    $site = upitt_workflow_get_object_fedora_sites($object_id);
    $parent = $batch['mapto_collections'];
    $site = $batch['mapto_site_id_values'];

    $batch_default_depositor = $batch['default_depositor'];

    $ingest_namespace = variable_get('upitt_workflow_ingest_namespace', 'pitt:');
    if ($fedora_model_name == 'bookCModel') {
      $markup .= upitt_workflow_drush_book($batch_path, $parent, $site, $ingest_namespace . $batch['batch_external_id']);
    } elseif ($fedora_model_name == 'sp_large_image_cmodel') {
      $markup .= upitt_workflow_csv2mods($batch_path, $fedora_model_name, $parent, $site, $batch_default_depositor);
    }

    $set = upitt_workflow_get_set($batch['batch_external_id']);

    $this_domain = 'http://' . $_SERVER['HTTP_HOST'];
    $ingest_drush_command = 'drush -v -u ' . $user->uid . ' --uri=' . $this_domain . ' islandora_batch_ingest';
    $ingest_set_drush_command = 'drush -v -u ' . $user->uid . ' --uri=' . $this_domain . ' islandora_batch_ingest --ingest_set=' . $set;

    // If this is a batch request, and the date is before the current date, this can ingest immediately.
    if ($batch['is_batch_request'] == 1 && strtotime($batch['batch_request_due_date']) < strtotime(date('Y-m-d'))) {
      // Ingest this NOW... 
      $output = $return_var = array();
      exec($ingest_drush_command, $output, $return_var);

      if (($return_var == -1 || $return_var == 255) && !$alerted_on_err) {
        drupal_set_message('There was an error calling the drush command : ' . $drush_command);
        $alerted_on_err = TRUE;
        $markup .= '<h3 style="color:red">Error calling ingest_batch command</h3>';
      } else {
        $markup .= '<p>Since the batch was set with a batch request due date before now, ' .
                   'the subsequent call to ingest the queued object/s has been executed.</p><code>' . $ingest_set_drush_command . '</code>';
      }
    } else {
      $markup .= '<p>Subsequent call to actually ingest the queued object/s.</p><code>' . $ingest_drush_command . '<br>' .
                 '<i># or if only one set to be processed, supply the ingest_set option</i><br>' . $ingest_set_drush_command .
                 '</code>';

      $markup .= '<h3>Batch values</h3>';
      $markup .= theme('workflow_item_details', array('details' => $batch));
    }
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
  // need to code some logic that scans the item_files to determine whether all of the requirements are satisified.
  return FALSE;
}

/**
 * The purpose of this function is to return several metrics about ingesting each fedora object type.  The result of this function will
 * return a relational array that contains three key sections:
 *
 *   'source_files' -- The source files that make up an ingestable object for the various object types.  This element contains values
 *       that are made up of target datastream value and source filename spec.
 *
 *   'required_datastreams' -- This is a list of all datastreams that should exist on a working object of this model type.  This list will
 *       frequently contain duplicate references to items listed under islandora_create_derivatives since the DRL system may have
 *       created the same datastream in the past, but we also want Islandora to generate it.
 *
 *   'islandora_create_derivatives' -- This is a list of any derivative datastreams that Islandora must create before this
 *       object has been fully ingested into Islandora.
 *
 *   'optional_passthrough_datastreams' -- This will point to any item_file reference that was created by DRL that we want to use
 *       without modification from the source item_files.  This value is an array (such as array(KEY=>VALUE,KEY2=VALUE2) where the KEY is
 *       the `use` from the item_file record and the array VALUE would be the datasteam identifier within Islandora.  A good example of
 *       this passthrough might be for the FITS.xml files which will be datatreams with id of `TECHMD` within Islandora.
 */
function upitt_workflow_fedora_object_requirements($fedoraObjectModel) {
  $source_files = $required_datastreams = $islandora_create_derivatives = $passthrough_datastreams = array();
  switch ($fedoraObjectModel) {
    case "sp_large_image_cmodel":
      $source_files[] = array('OBJ' => '*.tif');
      $required_datastreams[] = 'MODS';
      $required_datastreams[] = 'DC';
      $required_datastreams[] = 'JP2';
      $required_datastreams[] = 'JPG';
      $required_datastreams[] = 'THUMB';
      $required_datastreams[] = 'TN';
      $required_datastreams[] = 'TECHMD';
      $islandora_create_derivatives[] = 'DC';
      $islandora_create_derivatives[] = 'JP2';
      $islandora_create_derivatives[] = 'JPG';
      $islandora_create_derivatives[] = 'THUMB';
      $islandora_create_derivatives[] = 'TN';
      $islandora_create_derivatives[] = 'TECHMD';
      $passthrough_datastreams[] = array('TARGET' => 'TARGET');
      $passthrough_datastreams[] = array('FITS' => 'TECHMD');
      break;
    case "bookCModel":
      break;
    case "green":
    default:
  }
  return array('source_files' => $source_files,
               'required_datastreams' => $required_datastreams,
               'islandora_create_derivatives' => $islandora_create_derivatives,
               'passthrough_datastreams' => $passthrough_datastreams);
}

function upitt_workflow_sp_large_image_cmodel_create_object($pid) {
  drupal_set_message('<div style="color:#593">Called function upitt_workflow_sp_large_image_cmodel_create_object($pid="' . $pid . '");</div>');
}

function upitt_workflow_sp_large_image_cmodel_make_derivative($pid, $dsid) {
  drupal_set_message('<div style="color:#593">Called function upitt_workflow_sp_large_image_cmodel_make_derivative($pid="' . $pid . '", $dsid="' . $dsid . '");</div>');
}

/**
 * Helper function to generate MODS from a spreadsheet.
 */
function upitt_workflow_csv2mods($filepath, $fedora_model_name, $parent, $site, $batch_default_depositor) {
  global $user;
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $retval = '';
  // First step, look for a "*.csv" file in the filepath.
  // Get the path for the lib folder - a bit of a hack, but it's safe...
  $module_lib_path =  str_replace('/includes', '/lib', dirname(__FILE__));

  $files = upitt_workflow_file_scan_directory($filepath, 'metadata.csv');

  $this_domain = 'http://' . $_SERVER['HTTP_HOST'];

  $alerted_on_err = FALSE;
  foreach ($files as $file) {
    $csv_file_sources = upitt_workflow_prepare_csv2mods($filepath . '/' . $file, $batch_default_depositor);
    foreach ($csv_file_sources as $csv_file_source) {

      // TODO: make sure this row is not an empty line
      $drush_command = 'drush -v -u ' . $user->uid . ' --uri=' . $this_domain . ' islandora_batch_scan_preprocess ' .
                       '--content_models=islandora:' . $fedora_model_name . ' ' .
                       // assuming that only if the parent is set, than the parent_relationship_pred is needed
                       (($parent) ? '--parent=' . $parent . ' --parent_relationship_pred=isMemberOfCollection ' : '') .
                       '--type=directory --overwrite_existing=1 ' .
                       '--assign_pid=' . $csv_file_source['identifier'] . ' ' .
                       (($site) ? '--relationship=' . $site . ' --relationship_pred=isMemberOfSite --relationship_namespace=http://digital.library.pitt.edu/ontology/relations# ': '') .
                       ' --target=' . $csv_file_source['path'];
      $markup = '<h4>To call islandora_batch_scan_preprocess</h4><code>' . $drush_command . '</code>';
      $retval .= $markup;

      $output = $return_var = array();
      exec($drush_command, $output, $return_var);

      if (($return_var == -1 || $return_var == 255) && !$alerted_on_err) {
        drupal_set_message('There was an error calling the drush command : ' . $drush_command);
        $alerted_on_err = TRUE;
      }
    }
  }

  return $retval;
}

/**
 * Helper function to just issue the drush command.
 */
function upitt_workflow_drush_book($filepath, $parent, $site, $pid) {
  global $user;
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $retval = '';
  // First step, look for a "*.csv" file in the filepath.
  // Get the path for the lib folder - a bit of a hack, but it's safe...
  $module_lib_path =  str_replace('/includes', '/lib', dirname(__FILE__));

  $this_domain = 'http://' . $_SERVER['HTTP_HOST'];

  $alerted_on_err = FALSE;

  // Pending development in the islandora_book_ingest code, this will need to change to
  // pass parameter to aggregate OCR / PDF during ingetst
  $drush_command = 'drush -v -u ' . $user->uid . ' --namespace=pitt --uri=' . $this_domain . ' islandora_book_batch_preprocess ' .
                   '--content_models=islandora:bookCModel ' .
                   // assuming that only if the parent is set, than the parent_relationship_pred is needed
                   (($parent) ? '--parent=' . $parent . ' --parent_relationship_pred=isMemberOfCollection ' : '') .
                   '--type=directory ' .
                   '--assign_pid=' . $pid . ' ' .
                   '--overwrite_existing=1 ' .
                   (($site) ? '--relationship=' . $site . ' --relationship_pred=isMemberOfSite --relationship_namespace=http://digital.library.pitt.edu/ontology/relations# ': '') .
                   ' --target=' . $filepath;
  $markup = '<h4>To call islandora_batch_scan_preprocess</h4><code>' . $drush_command . '</code>';
  $retval .= $markup;

  $output = $return_var = array();
  exec($drush_command, $output, $return_var);
  
  if (($return_var == -1 || $return_var == 255) && !$alerted_on_err) {
    drupal_set_message('There was an error calling the drush command : ' . $drush_command);
    $alerted_on_err = TRUE;
  }

  return $retval;
}

/**
 * This will parse a row from a CSV file and return an array with two parts to the info:
 *
 * $prepared_filepath = The path to the folder that contains the MODS and image TIF.
 * $identifier - the identifier value (if provided in the spreadsheet).
 */
function upitt_workflow_prepare_csv2mods($input_filename, $default_depositor = '') {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  ini_set('auto_detect_line_endings', true);
  $outputFilename   = 'MODS.xml';

  // Open csv to read
  $inputFile  = fopen($input_filename, 'rt');

  // Get the headers of the file
  $headers = fgetcsv($inputFile);
  if ((array_search('identifier', $headers) === FALSE) and (array_search('Identifier', $headers) === FALSE)) {
    return FALSE;
  }

  $working_directory = dirname($input_filename);

  echo "<h3>Working directory " . $working_directory . "</h3>";

  $st = " style='background-color:#F2F2F2;border:1px solid #222'";  $toggle = false;  $row_counter = 1;

  $csv_header_xpath_mappings = upitt_workflow_get_csv_header_xpath_mappings();
  $mods_created = $files_copied = $bytes_written = 0;

  $ingest_namespace = variable_get('upitt_workflow_ingest_namespace', 'pitt:');
  $identifiers_and_filepaths = array();
  // Loop through each row creating a <row> node with the correct data
  while (($row = fgetcsv($inputFile)) !== FALSE) {
    // First, scan the entire row to get the identifier and the file_name value
    $toggle = !$toggle;
    echo '<div' . ($toggle ? $st : " style='border:1px solid #222'") . '>';
    echo "<h3>row " . $row_counter . "</h3>";
    $row_file_name = $row_mods_id = '';
    $rowcopy = $row;
    foreach($headers as $i => $header) {
      $header = str_replace(" ", "_", strtolower(trim($header)));
      // Some partner decided to end the row of headers with a few commas like "depositor,,,,," - so must
      // check that the header actually has a value.
      if ($header) {
        if (array_key_exists($i, $row)) {
          // ALL LOWERCASE, replace spaces with "_"
          if ($header == 'identifier') {
            $row_mods_id = $row[$i];
            $mods_directory_path = $working_directory . '/' . $row_mods_id;
            if (!file_exists($mods_directory_path)) {
              mkdir($mods_directory_path);
            }
          }
          elseif (($header == 'file_name' || $header == 'filename') && array_key_exists($i, $row)) {
            $row_file_name = $working_directory . '/' . $row[$i];
            $outputFilename = str_replace(array('.tiff', '.tif'), '.xml', $row[$i]);
          }
          elseif ($header == 'normalized_date' && array_key_exists($i, $row)) {
            list($sort_date, $date) = upitt_workflow_make_custom_csv_dates($row[$i], FALSE);
            // Add the sort_date field and solr 'date' field
            if ((array_key_exists('sort_date', $headers) === FALSE)) {
              $headers[] = 'sort_date';
              $rowcopy[] = $sort_date;
            }
            if ((array_key_exists('date', $headers) === FALSE)) {
              $headers[] = 'date';
              $rowcopy[] = $date;
            }
            upitt_workflow_fix_if_csv_date_approximate($headers, $rowcopy);
          }
        }
      }
      $headers[$i] = $header;
    }
    // echo "<pre>".print_r($rowcopy, true)."</pre>";

    // MOVE the file into the row's working directory
    if ($row_file_name) {
      if (file_exists($row_file_name)) {
        $command = "cp $row_file_name $working_directory" . '/' . $row_mods_id . '/.';
        exec($command);
        echo "COPY $row_file_name to $working_directory" . '/' . $row_mods_id ."/.<br>";
        $files_copied++;
      }
    }

    // Create a new dom document with pretty formatting
    $doc  = new DomDocument();
    $doc->formatOutput   = true;

    // Start creating the MODS XML by adding the root node to the document.
    $root = $doc->createElement('sheet'); // mods:mods'); // ,'test value');
    $root->appendChild($doc->createAttribute('xmlns:xsi'))->appendChild($doc->createTextNode("http://www.w3.org/2001/XMLSchema-instance"));

    $root = $doc->appendChild($root);
    $child = NULL;
    $depositor_in_CSV = FALSE;
    foreach($headers as $i => $header) {
      if (array_key_exists($header, $csv_header_xpath_mappings)) {
        if (is_array($rowcopy) && array_key_exists($i, $rowcopy) && $rowcopy[$i]) {
          $child = $doc->createElement($header);
          $child = $root->appendChild($child);
          $value = $doc->createTextNode($rowcopy[$i]);
          $value = $child->appendChild($value);
          if (!$depositor_in_CSV && $header == 'depositor') {
            $depositor_in_CSV = TRUE;
          }
        }
      }
    }
    if ($child) {
      $root->appendChild($child);
    }

    // After the CSV row has been processed, as long as the depositor value was not provided in that row,
    // and the batch provided a "default_depositor" value, then create this node now.
    if (!$depositor_in_CSV && $default_depositor && !array_key_exists('depositor', $headers)) {
      $child = $doc->createElement('depositor');
      $child = $root->appendChild($child);
      $value = $doc->createTextNode($default_depositor);
      $value = $child->appendChild($value);
    }
    $strxml = $doc->saveXML();

    echo "<blockquote><pre style='color:#c55'>".htmlspecialchars($strxml) ."</pre></blockquote>";

    $handle = fopen($working_directory . '/' . $row_mods_id . '/' . $outputFilename, "w");
    $bytes_written += fwrite($handle, $strxml);
    fclose($handle);
    echo "</div>";
    $filename = upitt_workflow_XMLtoMODS($working_directory . '/' . $row_mods_id . '/' . $outputFilename);
    $mods_created += ($filename <> '');
    echo "<p>created MODS from spreadsheet row = <b>" . $filename . "</b></p>";
    $path_parts = pathinfo($filename);

    $identifiers_and_filepaths[] = array('path' => $path_parts['dirname'], 'identifier' => $ingest_namespace . $row_mods_id);
    $row_counter++;
  }
  echo "<p>" . number_format($files_copied) . " files copied to holding directory <b>" . $working_directory . "</b></p>";
  echo "<p>" . number_format($mods_created) . " MODS files created.</p>";

  return $identifiers_and_filepaths;
}
