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
 *   1. To create base object:       "upitt_workflow_{FEDORA_MODEL_NAME}_create_object"
 *   2. To add a given datastream:   "upitt_workflow_{FEDORA_MODEL_NAME}_add_datastream"
 *   3. To generate a derivative:    "upitt_workflow_{FEDORA_MODEL_NAME}_make_derivative"
 *   4. To add relationships?:       "upitt_workflow_{FEDORA_MODEL_NAME}_add_relationship"
 */
function upitt_workflow_islandora_ingest($object_id) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  $batch = upitt_workflow_batch_of_item($object_id);

  $markup = '<p>Ingest single item "' . $object_id . '"';

//  $modsXML = upitt_workflow_csv2mods('/ingest/holding/MSS960_B015_F070_I001/');
//  $markup .= "<pre>" . htmlspecialchars($modsXML) . "</pre>";
  if ($batch) {
    $workflow_type_to_fedoramodel_mapping = upitt_workflow_get_workflow_to_fedora_object_mappings();

    // translate the batch's object type to a fedora object type for the requirements:
    $fedora_model_name = $workflow_type_to_fedoramodel_mapping[$batch['default_type_of_resource']];

    $markup .= ' from batch ' . $batch['batch_external_id'] . '</p>';

    $markup .= '<h3 style="color:green">Batch `default_type_of_resource` = "' . $batch['default_type_of_resource'] . '", yields fedora model: "' . $fedora_model_name . '"</h3><br>';

    $requirements = upitt_workflow_fedora_object_requirements($fedora_model_name);
    if ($requirements) {
      $markup  .= '<h3>Requirements for "' . $fedora_model_name . '"</h3><pre>' . print_r($requirements, true) . '</pre>';
    }

    $use_pid = 'pitt:' . $object_id;

    // For the drush call, we need to get a few values from the batch record and even lookup the collection PIDs and item_file path.    
    $parent = upitt_workflow_get_object_fedora_collections($object_id);
    $site = upitt_workflow_get_object_fedora_sites($object_id);

    $markup .= upitt_workflow_csv2mods('/ingest/holding/MSS960_B015_F070_I001/', $fedora_model_name, $parent, $site); 

    $item_files = upitt_workflow_get_item_files($object_id);
    $item_path = '/tmp/this/path/does/not/exist';
    if (isset($item_files[0]['path'])) {
      $parts = explode("/", $item_files[0]['path']);
      array_pop($parts);
      $item_path = implode("/", $parts);
    } 
    $markup .= upitt_workflow_item_file_fixnames($item_files, $use_pid, $batch['batch_external_id']);

    // NOTE -- the drush command does not yet support multiple parent values.  It will need to be enhanced so that it can 
    //         take a CSV value for our needs.  This will also need to be extened to set our IsMemberOfSite value.
    $drush_command = 'drush -v -u 1 --uri=http://dev.gamera.library.pitt.edu islandora_batch_scan_preprocess ' . 
                     '--content_models=islandora:' . $fedora_model_name . ' ' .
                     // assuming that only if the parent is set, than the parent_relationship_pred is needed
                     (($parent) ? '--parent=' . $parent . ' --parent_relationship_pred=isMemberOfCollection ' : '') . 
                     '--type=directory ' .
                     '--assign_pid=' . $use_pid . ' ' .
                     (($site) ? '--site=' . $site . ' --site_relationship_pred=isMemberOfSite ': '') . 
                     ' --target=' . $item_path;
    $markup .= '<h3>Drush call</h3><h4>To call islandora_batch_scan_preprocess</h4><code>' . $drush_command . '</code>';

    $markup .= '<p>Subsequent call to actually ingest the queued object/s.</p><code>drush -v -u 1 --uri=http://dev.gamera.library.pitt.edu islandora_batch_ingest --site=' . $site . '<br>' . 
               '<i># or if only one set to be processed, supply the ingest_set option</i><br>' .
               'drush -v -u 1 --uri=http://dev.gamera.library.pitt.edu islandora_batch_ingest --site=' . $site . ' --ingest_set=82</code>';

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
        $function_name = 'upitt_workflow_' . $fedora_model_name . '_create_object';
        $markup .= "<hr>Create object function call: <code>" . $function_name . "(\$pid = '" . $use_pid . "');</code> (" . ((function_exists($function_name)) ? 'exists' : 'no function') . ')';
        if (function_exists($function_name)) {
          $function_name($use_pid);
        }
      }
      
      // make derivatives job calls 
      $function_name = 'upitt_workflow_' . $fedora_model_name . '_make_derivative';
      foreach ($requirements['islandora_create_derivatives'] as $derivative_requirement) {
        $markup .= "<hr>Create derivatives function call: <code>" . $function_name . "(\$pid = '" . $use_pid . "', \$dsid = '" . $derivative_requirement . "');</code> " . 
                   "(" . ((function_exists($function_name)) ? 'exists' : 'no function') . ')';
        if (function_exists($function_name)) {
          $function_name($use_pid, $derivative_requirement);
        }
      }

      // passthrough datastream job calls
      $function_name = 'upitt_workflow_' . $fedora_model_name . '_add_datastream';
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

// returns markup to represent the file move / renaming 
function upitt_workflow_item_file_fixnames($item_files, $use_pid, $batch_external_id) {
  $foldername_pid = str_replace(':', '_', $use_pid);
  $markup = '';
  $files = array();  
  foreach ($item_files as $item_file) {
    $files[$item_file['use']] = $item_file;
  }
  // MASTER
  if (!isset($files['MASTER'])) {
    $markup .= '<b>MASTER file missing</b>';
  } else {
    $markup .= upitt_workflow_rename_file($files['MASTER'], 'OBJ.tif', $use_pid, $batch_external_id);
  }
  // MODS
  if (!isset($files['MODS'])) {
    $markup .= '<b>MODS file missing</b>';
  } else {
    $markup .= upitt_workflow_rename_file($files['MODS'], 'MODS.xml', $use_pid, $batch_external_id);
  }
  // DC
  if (!isset($files['DC'])) {
  } else {
    $markup .= upitt_workflow_rename_file($files['DC'], 'DC.bin', $use_pid, $batch_external_id);
  }
  // JP2
  if (!isset($files['JP2'])) {
    $markup .= '<b>JP2 file missing</b>';
  } else {
    $markup .= upitt_workflow_rename_file($files['JP2'], 'JP2.jp2', $use_pid, $batch_external_id);
  }

  // THUMB_LARGE
  if (!isset($files['THUMB_LARGE'])) {
  } else {
    $markup .= upitt_workflow_rename_file($files['THUMB_LARGE'], 'TN_LARGE.jpg', $use_pid, $batch_external_id);
  }

  // THUMB
  if (!isset($files['THUMB'])) {
  } else {
    $markup .= upitt_workflow_rename_file($files['THUMB'], 'TN.png', $use_pid, $batch_external_id);
  }

  return $markup;
}

/**
 * Helper function to rename a given file from a legacy workflow or given partner's files to the 
 * expected names that can be ingested using islandora_batch ingest calls.
 *
 * returns markup to represent the file move / renaming
 */
function upitt_workflow_rename_file($file_arr, $filename, $use_pid, $batch_external_id) {
  @list($batch_datepart, $batch_collectionnamepart) = explode("-", $batch_external_id);
  if (!$batch_collectionnamepart) {
    $batch_collectionnamepart = $batch_external_id;
  }
  $foldername_pid = str_replace(':', '_', $use_pid);
  $batch_pid = str_replace("pitt:", "", $use_pid);

  $batch_path = '/ingest/islandora_ingest/' . $batch_collectionnamepart;
  $batch_object_path = $batch_path . '/' . $batch_pid;

  // create the subfolders if needed
  $output = $return_var = array();
  if (!file_exists($batch_path)) {
    $command = 'mkdir ' . $batch_path;
    exec($command, $output, $return_var);   
    if (!file_exists($batch_object_path)) {
      $command = 'mkdir ' . $batch_object_path;
      exec($command, $output, $return_var);
    }
  }

  $command = 'cp ' . $file_arr['path'] . ' ' . $batch_object_path . '/' . $filename;
  exec($command, $output, $return_var);

  return '<code><b style="color:green">' . $command . '</b></code><br>' . "<pre style='color:#458'>" . print_r($output, true) . "<hr>" . print_r($return_var, true) . "<pre>";; 
}

/**
 * Helper function to generate MODS from a spreadsheet.
 */
function upitt_workflow_csv2mods($filepath, $fedora_model_name, $parent, $site) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  
  $retval = '';
  // First step, look for a "*.csv" file in the filepath.
  // Get the path for the lib folder - a bit of a hack, but it's safe...
  $module_lib_path =  str_replace('/includes', '/lib', dirname(__FILE__));

  $files = upitt_workflow_file_scan_directory($filepath, '.csv');
//  $paths_and_identifiers = array();
  foreach ($files as $file) {
    $csv_file_sources = upitt_workflow_prepare_csv2mods($filepath . '/' . $file);
    foreach ($csv_file_sources as $csv_file_source) {
//    $paths_and_identifiers[] = upitt_workflow_prepare_csv2mods($filepath . '/' . $file);
      $drush_command = 'drush -v -u 1 --uri=http://dev.gamera.library.pitt.edu islandora_batch_scan_preprocess ' .
                       '--content_models=islandora:' . $fedora_model_name . ' ' .
                       // assuming that only if the parent is set, than the parent_relationship_pred is needed
                       (($parent) ? '--parent=' . $parent . ' --parent_relationship_pred=isMemberOfCollection ' : '') .
                       '--type=directory ' .
                       '--assign_pid=' . $csv_file_source['identifier'] . ' ' .
                       (($site) ? '--site=' . $site . ' --site_relationship_pred=isMemberOfSite ': '') .
                       ' --target=' . $csv_file_source['path'];
      $markup = '<h4>To call islandora_batch_scan_preprocess</h4><code>' . $drush_command . '</code>';
      $retval .= $markup;
    }
  }

  
  return $retval;
}

/**
 * This will parse a row from a CSV file and return an array with two parts to the info:
 *
 * $prepared_filepath = The path to the folder that contains the MODS and image TIF.
 * $identifier - the identifier value (if provided in the spreadsheet).
 */
function upitt_workflow_prepare_csv2mods($input_filename) {
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
      if (isset($row[$i])) {
        // ALL LOWERCASE, replace spaces with "_"
        $header = str_replace(" ", "_", strtolower(trim($header)));
        if ($header == 'identifier') {
          $row_mods_id = $row[$i];
          $mods_directory_path = $working_directory . '/' . $row_mods_id;
          if (!file_exists($mods_directory_path)) {
            mkdir($mods_directory_path);
          }
        }
        elseif (($header == 'file_name' || $header == 'filename') && isset($row[$i])) {
          $row_file_name = $working_directory . '/' . $row[$i];
        }
        elseif ($header == 'normalized_date') {
          list($sort_date, $date) = upitt_workflow_make_custom_csv_dates($row[$i]);
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
      $headers[$i] = $header;
    }
    // echo "<pre>".print_r($rowcopy, true)."</pre>";

    // MOVE the file into the row's working directory
    if ($row_file_name) {
      if (file_exists($row_file_name)) {
        $command = "cp $row_file_name to $working_directory" . '/' . $row_mods_id;
        exec($command);
        echo "COPY $row_file_name to $working_directory" . '/' . $row_mods_id ."<br>";
        $files_copied++;
      }
    }

    // Create a new dom document with pretty formatting
    $doc  = new DomDocument();
    $doc->formatOutput   = true;

    // Start creating the MODS XML by adding the root node to the document.
    $root = $doc->createElement('sheet'); // mods:mods'); // ,'test value');
    $root->appendChild($doc->createAttribute('xmlns:xsi'))->appendChild($doc->createTextNode("http://www.w3.org/2001/XMLSchema-instance"));
//    $root->appendChild($doc->createAttribute('xsi:schemaLocation'))->appendChild($doc->createTextNode('http://www.loc.gov/mods/v3 http://www.loc.gov/standards/mods/v3/mods-3-3.xsd http://www.cdlib.org/inside/diglib/copyrightMD'));

    $root = $doc->appendChild($root);

    foreach($headers as $i => $header) {
      if (array_key_exists($header, $csv_header_xpath_mappings)) {
        $mapping = $csv_header_xpath_mappings[$header];
        if (is_array($rowcopy) && isset($rowcopy[$i]) && $rowcopy[$i]) {
          $child = $doc->createElement($header);
          $child = $root->appendChild($child);
          $value = $doc->createTextNode($rowcopy[$i]);
          $value = $child->appendChild($value);
        }
      }
    }
    $root->appendChild($child);
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

    $identifiers_and_filepaths[] = array('path' => $path_parts['dirname'], 'identifier' => 'pitt:' . $row_mods_id);
    $row_counter++;
  }
  echo "<p>" . number_format($files_copied) . " files copied to holding directory <b>" . $working_directory . "</b></p>";
  echo "<p>" . number_format($mods_created) . " MODS files created.</p>";

  return $identifiers_and_filepaths;
}

/**
 * This still provides a test as to whether or not a field gets into the initial XML from the sheet - and the headings title
 * that would match for that field.
 */
function upitt_workflow_get_csv_header_xpath_mappings() {
  return array('genre'                     => '/mods:mods/mods:genre',
//               'description'               => '/mods:mods/mods:abstract[1]',
               'description'               => '/mods:mods/mods:abstract',
               'type_of_resource'          => '/mods:mods/mods:typeOfResource',
               'gift_of'                   => '/mods:mods/mods:note[@type="donor"]',
               'address'                   => '/mods:mods/mods:note[@type="address"]',
//               'subject'                   => '/mods:mods/mods:subject/mods:topic[1]',
               'subject'                   => '/mods:mods/mods:subject/mods:topic',
//               'contributor'               => '/mods:mods/mods:name[2]/mods:namePart[1]',
               'contributor'               => '/mods:mods/mods:name/mods:namePart[../mods:role/mods:roleTerm[(text()="contributor") and @type="text"]]',
//               'creator'                   => '/mods:mods/mods:name[1]/mods:namePart[1]',
               'creator'                   => '/mods:mods/mods:name/mods:namePart[../mods:role/mods:roleTerm[(text()="creator") and @type="text"]]',
//               'depositor'                 => '/mods:mods/mods:name[2]/mods:namePart[1]',
               'depositor'                 => '/mods:mods/mods:name/mods:namePart[../mods:role/mods:roleTerm[(text()="depositor") and @type="text"]]',
               'identifier'                => '/mods:mods/mods:identifier[@type="pitt"]',
               'source_id'                 => '/mods:mods/mods:identifier[@type="source"]',
               'source_identifier'         => '/mods:mods/mods:identifier[@type="source"]',
               'title'                     => '/mods:mods/mods:titleInfo/mods:title',
               'publisher'                 => '/mods:mods/mods:originInfo/mods:publisher',
               'date_digitized'            => '/mods:mods/mods:originInfo/mods:dateCaptured',
               'format'                    => '/mods:mods/mods:physicalDescription/mods:form',
               'subject_name'              => '/mods:mods/mods:subject[@authority="lcsh"]/mods:name',
               'dimension'                 => '/mods:mods/mods:physicalDescription/mods:extent',
               'source_citation'           => '/mods:mods/mods:relatedItem/note[@type="prefercite"]',
               'subject_lcsh'              => '/mods:mods/mods:subject[@authority="lcsh"]/mods:topic',
               'source_collection_id'      => '/mods:mods/mods:relatedItem[@type="host"]/mods:identifier',
               'subject_local'             => '/mods:mods/mods:subject[@authority="local"]/mods:topic',
               'sort_date'                 => '/mods:mods/mods:originInfo/mods:dateOther[@type="sort"]',
               'subject_location'          => '/mods:mods/mods:subject[@authority="lcsh"]/mods:geographic',
               'date'                      => '/mods:mods/mods:originInfo/mods:dateOther[@type="display"]',
               'scale'                     => '/mods:mods/mods:subject/mods:cartographics/mods:scale',
               'source_collection'         => '/mods:mods/mods:relatedItem/mods:titleInfo/mods:title',
               'copyright_status'          => '/mods:mods/mods:accessCondition/copyrightMD:copyright/@copyright.status',
               'source_container'          => '/mods:mods/mods:relatedItem[@type="host"]/mods:note[@type="container"]',
               'source_ownership'          => '/mods:mods/mods:relatedItem[@type="host"]/mods:note[@type="ownership"]',
               'publication_status'        => '/mods:mods/mods:accessCondition/copyrightMD:copyright/@publication.status',
               'pub_place'                 => '/mods:mods/mods:originInfo/mods:place/mods:placeTerm[@type="text"]',
               'source_collection_date'    => '/mods:mods/mods:relatedItem[@type="host"]/mods:originInfo/mods:dateCreated',
               'normalized_date'           => '/mods:mods/mods:originInfo/mods:dateCreated[@encoding="iso8601" @keyDate="yes"]',
               'rights_holder'             => '/mods:mods/mods:accessCondition/copyrightMD:copyright/copyrightMD:rights.holder/copyrightMD:name',
               'normalized_date_qualifier' => '/mods:mods/mods:originInfo/mods:dateCreated[@qualifier="approximate" @encoding="iso8601" @keyDate="yes"]',
// 'batch' => '',
// 'collection' => '',
// 'date_qualifier' => '',
// 'file_name' => '',
// 'location' => '',
    );

}
