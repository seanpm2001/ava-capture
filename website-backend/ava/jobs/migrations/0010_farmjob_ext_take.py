# -*- coding: utf-8 -*-
# Generated by Django 1.10.1 on 2017-03-22 19:27
from __future__ import unicode_literals

from django.db import migrations, models
import django.db.models.deletion


class Migration(migrations.Migration):

    dependencies = [
        ('archive', '0007_auto_20170320_1532'),
        ('jobs', '0009_farmjob_progress'),
    ]

    operations = [
        migrations.AddField(
            model_name='farmjob',
            name='ext_take',
            field=models.ForeignKey(blank=True, null=True, on_delete=django.db.models.deletion.SET_NULL, related_name='ext_jobs', to='archive.Take'),
        ),
    ]
