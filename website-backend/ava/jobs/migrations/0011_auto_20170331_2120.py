# -*- coding: utf-8 -*-
# Generated by Django 1.10.1 on 2017-04-01 01:20
from __future__ import unicode_literals

from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ('jobs', '0010_farmjob_ext_take'),
    ]

    operations = [
        migrations.AddField(
            model_name='farmjob',
            name='end_time',
            field=models.DateTimeField(null=True, verbose_name='end time'),
        ),
        migrations.AddField(
            model_name='farmjob',
            name='start_time',
            field=models.DateTimeField(null=True, verbose_name='start time'),
        ),
    ]
