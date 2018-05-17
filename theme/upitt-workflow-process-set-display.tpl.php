<?php if (count($items) > 0) : ?>
    <div class="process_set">
        <h3><?php print $title; ?> (<?php print number_format(count($items)); ?>)</h3>
        <?php print l('Process this set', 'workflow/workflowadmin/process_set/' .
            $process_set_id, array('attributes' => array('target' => '_blank'))); ?>
        <textarea rows="6" readonly class="short-text-report"><?php print implode("\n", $items); ?></textarea>
    </div>
<?php endif; ?>