# -*- coding: utf-8 -*-
# Generated by Django 1.10.1 on 2017-04-07 18:54
from __future__ import unicode_literals

from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ('jobs', '0015_farmjob_ext_scan_assets'),
    ]

    operations = [
        migrations.AddField(
            model_name='farmjob',
            name='req_gpu',
            field=models.BooleanField(default=True),
        ),
    ]
